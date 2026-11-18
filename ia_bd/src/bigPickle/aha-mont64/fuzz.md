# Teste diferencial aha-mont64: bigPickle vs original

Driver: `tests/aritmetic/aha-mont64.c`, n=2000, sementes [1, 2, 3]

| build | semente | linhas | iguais | observação |
|---|---|---|---|---|
| normal | 1 | 8203 | S |  |
| normal | 2 | 8203 | S |  |
| normal | 3 | 8203 | S |  |
| sanitizers | 1 | 8203 | S |  |
| sanitizers | 2 | 8203 | S |  |
| sanitizers | 3 | 8203 | S |  |

**Resultado geral: IGUAIS**