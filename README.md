# gmp-calculator
Calculator that uses recursive descent to parse arithmetic expressions and stores numbers as GMP (GNU multiple precision arithmetic library) types to calculate with arbitrary precision. Uses GNU Readline for input with in-line editing and history.

Arithmetic expressions are first tokenized in a way that ignores whitespace and stored in a linked list of tokens: `>> 3 + 3*2` --> `(INTEGER 3), (PLUS), (INTEGER 3), (STAR), (INTEGER 2)`

Then this list is parsed using recursive descent according to the following grammar:
```
expr   := term { ("+" | "-") expr }
term   := factor { ("*" | "/") factor }
factor := atom { "^" atom }
atom   := number | "(" expr ")"
```
which generates the bytecode:
`OP_PUSH_INT 3, OP_PUSH_INT 3, OP_PUSH_INT 2, OP_MUL, OP_ADD`
which is then evaluated by a stack based interpreter.

The expression `3 * 3 + 2` gets parsed into the bytecode:
`OP_PUSH_INT 3, OP_PUSH_INT 3, OP_MUL, OP_PUSH_INT 2, OP_ADD,`
which respects order of operations.
