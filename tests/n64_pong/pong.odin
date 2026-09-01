#+feature global-context

package n64_pong

import "base:runtime"
import ld "vendor:libdragon"

#assert(ODIN_OS == .N64)

READY_SENTINEL         :: "ODIN_N64_PONG_READY:v1\n"
INPUT_DOWN_SENTINEL    :: "ODIN_N64_PONG_INPUT:v1:DOWN\n"
INPUT_UP_SENTINEL      :: "ODIN_N64_PONG_INPUT:v1:UP\n"
INPUT_STICK_SENTINEL   :: "ODIN_N64_PONG_INPUT:v1:STICK\n"
INPUT_A_SENTINEL       :: "ODIN_N64_PONG_INPUT:v1:A\n"
STATE_SENTINEL         :: "ODIN_N64_PONG_STATE:v1:CHECKPOINT\n"
RALLY_SENTINEL         :: "ODIN_N64_PONG_STATE:v1:RALLY_STARTED\n"
GENERAL_SENTINEL       :: "ODIN_N64_PONG_CHECK:v1:GENERAL_ALLOCATOR:PASS\n"
TEMP_SENTINEL          :: "ODIN_N64_PONG_CHECK:v1:TEMP_ALLOCATOR:PASS\n"
TICKS_SENTINEL         :: "ODIN_N64_PONG_CHECK:v1:TICKS_ADVANCED:PASS\n"
PASS_SENTINEL          :: "ODIN_N64_PONG_PASS:v1\n"
FAIL_DISPLAY_SENTINEL  :: "ODIN_N64_PONG_FAIL:v1:DISPLAY_GET\n"
FAIL_GENERAL_SENTINEL  :: "ODIN_N64_PONG_FAIL:v1:GENERAL_ALLOCATOR\n"
FAIL_TEMP_SENTINEL     :: "ODIN_N64_PONG_FAIL:v1:TEMP_ALLOCATOR\n"
FAIL_TEMP_RESET        :: "ODIN_N64_PONG_FAIL:v1:TEMP_RESET\n"

SCREEN_WIDTH          :: 320
FIELD_LEFT            :: 12
FIELD_RIGHT           :: 308
FIELD_TOP             :: 30
FIELD_BOTTOM          :: 218
PADDLE_WIDTH          :: 8
PADDLE_HEIGHT         :: 36
PLAYER_X              :: 24
CPU_X                 :: 288
BALL_SIZE             :: 8
FIXED_STEP_MS         :: u64(16)
MAX_FRAME_MS          :: u64(64)
DASH_CAPACITY         :: 16
WINNING_SCORE         :: 5

Game_State :: struct {
	player_y:                 i32,
	cpu_y:                    i32,
	ball_x:                   i32,
	ball_y:                   i32,
	ball_vx:                  i32,
	ball_vy:                  i32,
	player_score:             i32,
	cpu_score:                i32,
	previous_buttons:         u16,
	accumulated_ms:           u64,
	ball_waiting:             bool,
	match_over:               bool,
	input_seen:               bool,
	checkpoint_reached:       bool,
	checkpoint_frame_shown:   bool,
	pass_emitted:             bool,
	rally_state_emitted:      bool,
	checkpoint_target_y:      i32,
}

Player_Input :: struct {
	move_y:         i32,
	buttons:        u16,
	pressed:        u16,
	directional:    bool,
	direction_name: Direction_Name,
}

Direction_Name :: enum {
	None,
	Down,
	Up,
	Stick,
}

@(private="file")
clamp_i32 :: proc(value, minimum, maximum: i32) -> i32 {
	if value < minimum {
		return minimum
	}
	if value > maximum {
		return maximum
	}
	return value
}

@(private="file")
initial_game_state :: proc() -> Game_State {
	return Game_State{
		player_y = 102,
		cpu_y = 102,
		ball_x = 156,
		ball_y = 116,
		ball_vx = 3,
		ball_vy = 2,
		ball_waiting = true,
	}
}

@(private="file", require_results)
allocate_game_state :: proc() -> (^Game_State, runtime.Allocator_Error) {
	memory, err := runtime.mem_alloc(size_of(Game_State), align_of(Game_State))
	if err != .None {
		return nil, err
	}
	state := (^Game_State)(raw_data(memory))
	state^ = initial_game_state()
	return state, .None
}

@(private="file")
read_player_input :: proc "contextless" (inputs: ld.joypad_inputs_t, previous_buttons: u16) -> Player_Input {
	buttons := inputs.btn.raw
	result := Player_Input{
		buttons = buttons,
		pressed = buttons & ~previous_buttons,
	}

	if buttons & ld.JOYPAD_BUTTON_D_DOWN != 0 {
		result.move_y = 1
		result.directional = true
		result.direction_name = .Down
	} else if buttons & ld.JOYPAD_BUTTON_D_UP != 0 {
		result.move_y = -1
		result.directional = true
		result.direction_name = .Up
	} else if inputs.stick_y < -20 {
		result.move_y = 1
		result.directional = true
		result.direction_name = .Stick
	} else if inputs.stick_y > 20 {
		result.move_y = -1
		result.directional = true
		result.direction_name = .Stick
	}
	return result
}

@(private="file")
emit_direction_sentinel :: proc "contextless" (direction: Direction_Name) {
	switch direction {
	case .Down:
		ld.debugf(INPUT_DOWN_SENTINEL)
	case .Up:
		ld.debugf(INPUT_UP_SENTINEL)
	case .Stick:
		ld.debugf(INPUT_STICK_SENTINEL)
	case .None:
	}
}

@(private="file")
begin_controller_checkpoint :: proc(state: ^Game_State, input: Player_Input) {
	state.input_seen = true
	state.checkpoint_target_y = input.move_y > 0 ? FIELD_BOTTOM-PADDLE_HEIGHT : FIELD_TOP
}

@(private="file")
reset_ball :: proc(state: ^Game_State, toward_player: bool) {
	state.ball_x = 156
	state.ball_y = 116
	state.ball_vx = toward_player ? -3 : 3
	state.ball_vy = state.player_score & 1 == 0 ? 2 : -2
	state.ball_waiting = true
}

@(private="file")
reset_match :: proc(state: ^Game_State) {
	state.player_score = 0
	state.cpu_score = 0
	state.match_over = false
	reset_ball(state, false)
}

@(private="file")
ball_overlaps_paddle :: proc(ball_x, ball_y, paddle_x, paddle_y: i32) -> bool {
	return ball_x < paddle_x + PADDLE_WIDTH &&
	       ball_x + BALL_SIZE > paddle_x &&
	       ball_y < paddle_y + PADDLE_HEIGHT &&
	       ball_y + BALL_SIZE > paddle_y
}

@(private="file")
bounce_ball_from_walls :: proc(state: ^Game_State) {
	if state.ball_y <= FIELD_TOP {
		state.ball_y = FIELD_TOP
		state.ball_vy = -state.ball_vy
	} else if state.ball_y + BALL_SIZE >= FIELD_BOTTOM {
		state.ball_y = FIELD_BOTTOM - BALL_SIZE
		state.ball_vy = -state.ball_vy
	}
}

@(private="file")
bounce_ball_from_paddles :: proc(state: ^Game_State) {
	if state.ball_vx < 0 && ball_overlaps_paddle(state.ball_x, state.ball_y, PLAYER_X, state.player_y) {
		state.ball_x = PLAYER_X + PADDLE_WIDTH
		state.ball_vx = -state.ball_vx
		state.ball_vy += (state.ball_y + BALL_SIZE/2 - (state.player_y + PADDLE_HEIGHT/2)) / 8
		state.ball_vy = clamp_i32(state.ball_vy, -4, 4)
	} else if state.ball_vx > 0 && ball_overlaps_paddle(state.ball_x, state.ball_y, CPU_X, state.cpu_y) {
		state.ball_x = CPU_X - BALL_SIZE
		state.ball_vx = -state.ball_vx
		state.ball_vy += (state.ball_y + BALL_SIZE/2 - (state.cpu_y + PADDLE_HEIGHT/2)) / 8
		state.ball_vy = clamp_i32(state.ball_vy, -4, 4)
	}
}

@(private="file")
score_if_ball_left_field :: proc(state: ^Game_State) {
	if state.ball_x + BALL_SIZE < FIELD_LEFT {
		state.cpu_score += 1
		if state.cpu_score >= WINNING_SCORE {
			state.match_over = true
		}
		reset_ball(state, false)
	} else if state.ball_x > FIELD_RIGHT {
		state.player_score += 1
		if state.player_score >= WINNING_SCORE {
			state.match_over = true
		}
		reset_ball(state, true)
	}
}

@(private="file")
step_playing_game :: proc(state: ^Game_State, input: Player_Input) {
	state.player_y += input.move_y * 4
	state.player_y = clamp_i32(state.player_y, FIELD_TOP, FIELD_BOTTOM-PADDLE_HEIGHT)
	if state.ball_waiting || state.match_over {
		return
	}

	ball_center := state.ball_y + BALL_SIZE/2
	cpu_center := state.cpu_y + PADDLE_HEIGHT/2
	if ball_center < cpu_center-2 {
		state.cpu_y -= 2
	} else if ball_center > cpu_center+2 {
		state.cpu_y += 2
	}
	state.cpu_y = clamp_i32(state.cpu_y, FIELD_TOP, FIELD_BOTTOM-PADDLE_HEIGHT)

	state.ball_x += state.ball_vx
	state.ball_y += state.ball_vy
	bounce_ball_from_walls(state)
	bounce_ball_from_paddles(state)
	score_if_ball_left_field(state)
}

@(private="file")
apply_game_buttons :: proc(state: ^Game_State, input: Player_Input) {
	if input.pressed & ld.JOYPAD_BUTTON_START != 0 {
		reset_match(state)
	}
	if input.pressed & ld.JOYPAD_BUTTON_A != 0 {
		if state.match_over {
			reset_match(state)
		}
		state.ball_waiting = false
	}
}

@(private="file")
advance_game :: proc(state: ^Game_State, input: Player_Input, elapsed_ms: u64) {
	apply_game_buttons(state, input)
	state.accumulated_ms += elapsed_ms
	for state.accumulated_ms >= FIXED_STEP_MS {
		step_playing_game(state, input)
		state.accumulated_ms -= FIXED_STEP_MS
	}
}

@(private="file")
draw_game_frame :: proc "contextless" (
	frame: ^ld.surface_t,
	state: ^Game_State,
	dash_positions: [^]i32,
	dash_count: int,
) {
	background := ld.graphics_make_color(8, 14, 28, 255)
	field := ld.graphics_make_color(60, 86, 112, 255)
	player := ld.graphics_make_color(70, 224, 144, 255)
	cpu := ld.graphics_make_color(248, 116, 96, 255)
	ball := ld.graphics_make_color(255, 224, 96, 255)
	text := ld.graphics_make_color(240, 244, 248, 255)

	ld.graphics_fill_screen(frame, background)
	ld.graphics_draw_box(frame, FIELD_LEFT, FIELD_TOP, FIELD_RIGHT-FIELD_LEFT, 3, field)
	ld.graphics_draw_box(frame, FIELD_LEFT, FIELD_BOTTOM-3, FIELD_RIGHT-FIELD_LEFT, 3, field)
	for index in 0..<dash_count {
		ld.graphics_draw_box(frame, SCREEN_WIDTH/2-1, dash_positions[index], 3, 8, field)
	}
	ld.graphics_draw_box(frame, PLAYER_X, state.player_y, PADDLE_WIDTH, PADDLE_HEIGHT, player)
	ld.graphics_draw_box(frame, CPU_X, state.cpu_y, PADDLE_WIDTH, PADDLE_HEIGHT, cpu)
	ld.graphics_draw_box(frame, state.ball_x, state.ball_y, BALL_SIZE, BALL_SIZE, ball)
	for pip in 0..<state.player_score {
		ld.graphics_draw_box(frame, 126+i32(pip)*7, 12, 5, 8, player)
	}
	for pip in 0..<state.cpu_score {
		ld.graphics_draw_box(frame, 166+i32(pip)*7, 12, 5, 8, cpu)
	}
	ld.graphics_set_default_font()
	ld.graphics_set_color(text, background)
	ld.graphics_draw_text(frame, 12, 222, "MOVE PADDLE  A SERVE  START RESET")
}

@(private="file")
present_frame :: proc(state: ^Game_State) -> bool {
	frame := ld.display_get()
	if frame == nil {
		ld.debugf(FAIL_DISPLAY_SENTINEL)
		return false
	}
	memory, err := runtime.mem_alloc(size_of(i32)*DASH_CAPACITY, align_of(i32), context.temp_allocator)
	if err != .None {
		ld.debugf(FAIL_TEMP_SENTINEL)
		return false
	}
	dash_positions := ([^]i32)(raw_data(memory))
	dash_count := 0
	for dash_y := FIELD_TOP+8; dash_y < FIELD_BOTTOM-8; dash_y += 16 {
		dash_positions[dash_count] = i32(dash_y)
		dash_count += 1
	}
	draw_game_frame(frame, state, dash_positions, dash_count)
	ld.display_show(frame)
	if runtime.mem_free_all(context.temp_allocator) != .None {
		ld.debugf(FAIL_TEMP_RESET)
		return false
	}
	return true
}

main :: proc() {
	_ = ld.debug_init_emulog()
	state, allocation_error := allocate_game_state()
	if allocation_error != .None || state == nil {
		ld.debugf(FAIL_GENERAL_SENTINEL)
		return
	}
	ld.debugf(GENERAL_SENTINEL)

	ld.joypad_init()
	ld.display_init(ld.RESOLUTION_320x240, .DEPTH_16_BPP, 2, .GAMMA_NONE, .FILTERS_DISABLED)
	if !present_frame(state) {
		return
	}
	ld.debugf(TEMP_SENTINEL)
	ld.debugf(READY_SENTINEL)

	started_ms := ld.get_ticks_ms()
	last_ms := started_ms
	ticks_reported := false
	for {
		ld.joypad_poll()
		raw_input := ld.joypad_get_inputs(.JOYPAD_PORT_1)
		input := read_player_input(raw_input, state.previous_buttons)
		state.previous_buttons = input.buttons
		if input.pressed & ld.JOYPAD_BUTTON_A != 0 {
			ld.debugf(INPUT_A_SENTINEL)
		}

		now_ms := ld.get_ticks_ms()
		elapsed_ms := now_ms - last_ms
		last_ms = now_ms
		if elapsed_ms > MAX_FRAME_MS {
			elapsed_ms = MAX_FRAME_MS
		}
		if !ticks_reported && now_ms > started_ms {
			ld.debugf(TICKS_SENTINEL)
			ticks_reported = true
		}

		if !state.input_seen && input.directional {
			emit_direction_sentinel(input.direction_name)
			begin_controller_checkpoint(state, input)
		}
		previous_player_y := state.player_y
		previous_cpu_y := state.cpu_y
		previous_ball_x := state.ball_x
		previous_ball_y := state.ball_y
		previous_player_score := state.player_score
		previous_cpu_score := state.cpu_score
		previous_ball_waiting := state.ball_waiting
		previous_match_over := state.match_over
		advance_game(state, input, elapsed_ms)
		if state.input_seen && state.player_y == state.checkpoint_target_y {
			state.checkpoint_reached = true
		}

		frame_changed :=
			state.player_y != previous_player_y ||
			state.cpu_y != previous_cpu_y ||
			state.ball_x != previous_ball_x ||
			state.ball_y != previous_ball_y ||
			state.player_score != previous_player_score ||
			state.cpu_score != previous_cpu_score ||
			state.ball_waiting != previous_ball_waiting ||
			state.match_over != previous_match_over
		if frame_changed {
			if !present_frame(state) {
				return
			}
		}
		if frame_changed && state.checkpoint_reached && !state.checkpoint_frame_shown {
			state.checkpoint_frame_shown = true
			ld.debugf(STATE_SENTINEL)
		}
		if state.checkpoint_frame_shown && !state.pass_emitted && ticks_reported {
			state.pass_emitted = true
			ld.debugf(PASS_SENTINEL)
		}
		if frame_changed && !state.ball_waiting &&
		   state.ball_x != previous_ball_x && !state.rally_state_emitted {
			state.rally_state_emitted = true
			ld.debugf(RALLY_SENTINEL)
		}
	}
}
