#!/usr/bin/env sh
# Builtins read a record's members through type_struct_fields and type_union_variants, which wait until
# another thread has finished checking a polymorphic instantiation. Only stores into a record the
# builtin builds itself may name the members directly.

file=src/check_builtin.cpp
if [ ! -f "$file" ]; then
	echo "$file not found; run from the repository root"
	exit 1
fi

reads=$(grep -nE '(Struct\.fields|Union\.variants)([^_A-Za-z0-9]|$)' "$file" \
	| sed -E 's/[A-Za-z_]+->(Struct\.fields|Union\.variants)(\[[^]]*\])? = //' \
	| grep -E '(Struct\.fields|Union\.variants)([^_A-Za-z0-9]|$)')

if [ -n "$reads" ]; then
	echo "$file reads record members directly; use type_struct_fields or type_union_variants:"
	echo "$reads"
	exit 1
fi
