#!/usr/bin/env bash
# Usage: tests/run_tests.sh   (from the project root, after `make`)
cd "$(dirname "$0")/.." || exit 2
BIN=./bminor
pass=0; fail=0
ok()  { pass=$((pass+1)); echo "  ok   $1"; }
bad() { fail=$((fail+1)); echo "  FAIL $1"; }

echo "== valid programs: exit 0, no diagnostics"
for f in tests/valid/*.bm; do
  if $BIN --ast --tokens "$f" >/dev/null 2>/tmp/err.txt && [ ! -s /tmp/err.txt ]; then ok "$f"; else bad "$f"; cat /tmp/err.txt; fi
done

echo "== invalid programs: exit 1 (diagnostics, never a crash)"
for f in tests/invalid/*.bm; do
  $BIN --ast --html /tmp/out.html "$f" >/dev/null 2>/tmp/err.txt; rc=$?
  if [ $rc -eq 1 ] && [ -s /tmp/err.txt ]; then ok "$f ($(wc -l </tmp/err.txt) diagnostics)"; else bad "$f rc=$rc"; fi
done

echo "== lexer rules"
t=$(printf 'a <= b >= c == d != e && f || g ++ -- return returned true' | $BIN --tokens -)
for p in 'OP(<=)' 'OP(>=)' 'OP(==)' 'OP(!=)' 'OP(&&)' 'OP(||)' 'OP(++)' 'OP(--)' 'KW(return)' 'IDENT(returned)' 'BOOL(true)'; do
  echo "$t" | grep -qF "$p" && ok "token $p" || bad "token $p"
done
echo "$t" | grep -qF 'IDENT(return)' && bad "keyword priority" || ok "keyword priority"
echo "$t" | grep -qF 'OP(<)' && bad "maximal munch" || ok "maximal munch"
t=$(printf 'x: integer = 5@;\ny: integer = 10;' | $BIN --tokens - 2>/dev/null)
echo "$t" | grep -qF '1:15	INVALID(@)' && ok "INVALID(@) at 1:15" || bad "INVALID(@) at 1:15"
echo "$t" | grep -qF 'INT(10)' && ok "scan resumes after invalid char" || bad "scan resumes"

echo "== parser recovery"
n=$($BIN tests/invalid/parse_errors.bm 2>&1 >/dev/null | grep -c 'syntax error')
[ "$n" -ge 6 ] && ok "multiple syntax errors reported in one run ($n)" || bad "recovery ($n)"
$BIN --ast tests/invalid/parse_errors.bm 2>/dev/null | grep -q 'VarDecl(w)' && ok "declaration after errors still parsed" || bad "post-error decl"

echo "== AST-level highlighting"
$BIN --html /tmp/f.html tests/valid/factorial.bm
[ "$(grep -o 'class="fn">factorial' /tmp/f.html | wc -l)" -eq 2 ] && ok "declaration and call of factorial -> fn" || bad "fn class"
grep -q '<span class="var">n</span>' /tmp/f.html && ok "variable -> var" || bad "var class"
grep -q '<span class="ty">integer</span>' /tmp/f.html && ok "type -> ty" || bad "ty class"
grep -q '<span class="kw">return</span>' /tmp/f.html && ok "keyword -> kw" || bad "kw class"
$BIN tests/valid/factorial.bm | grep -q $'\x1b\\[1;34mreturn' && ok "ANSI bold blue keyword" || bad "ANSI keyword"
# the printed output must reproduce the source exactly once escapes are removed
for f in tests/valid/*.bm tests/invalid/*.bm; do
  $BIN "$f" 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | cmp -s - "$f" || { bad "round-trip $f"; continue; }
done
ok "ANSI round-trip (colors stripped == source)"

echo; echo "passed=$pass failed=$fail"
[ $fail -eq 0 ]
