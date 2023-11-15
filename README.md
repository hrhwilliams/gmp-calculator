# gmp-calculator
Calculator that uses recursive descent to parse arithmetic expressions and stores numbers as GMP (GNU multiple precision arithmetic library) types to calculate with arbitrary precision. Uses GNU Readline for input with in-line editing and history.

Arithmetic expressions are first tokenized in a way that ignores whitespace and stored in a linked list of tokens: `>> 3 + 3*2` --> `(INTEGER 3), (PLUS), (INTEGER 3), (STAR), (INTEGER 2)`

Then this list is parsed using recursive descent according to the following grammar:
```
expr   := term { (“+” | “-”) expr }
term   := factor { (“*” | “/” | “%”) factor }
factor := atom { “^” atom }
atom   := number | “(” expr “)” 
```
which generates the bytecode:
`OP_PUSH_INT 3, OP_PUSH_INT 3, OP_PUSH_INT 2, OP_MUL, OP_ADD`
which is then evaluated by a stack based interpreter.

The expression `3 * 3 + 2` gets parsed into the bytecode:
`OP_PUSH_INT 3, OP_PUSH_INT 3, OP_MUL, OP_PUSH_INT 2, OP_ADD,`
which respects order of operations.

Because the calculator uses GMP, it is able to evaluate expressions with arbitrarily large answers:

```
>> 2^2203 + 1
1475979915214180235084898622737381736312066145333169775147771216478570
2978780789493774073370493892893827485075314964804772812648387602591918
1446336533026954049696120111343015690239609398909022625932693502528140
9614983499388222831448598601834318536230923772641390209490231836446899
6082107954829637630942366309454108327937699053999824571863229447296364
1889062337217172374210563644036821845964963294853869690587265048691443
4637457507280441823676813517852099348660847172579408422316678097670224
0119902801704748944874269247421088235368084850725022405194525875428753
4997655857267022963396257521263747789778550155264652260998886991401354
0483809865681250419497686697771009
```

```
>> (3^2203 - 3) % 2203
0
```
