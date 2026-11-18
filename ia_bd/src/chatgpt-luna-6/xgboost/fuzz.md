# Teste diferencial xgboost: chatgpt-luna-6 vs original

Driver: `tests/control/xgboost_test.c`, n=2000, sementes [1, 2, 3]

| build | semente | linhas | iguais | observação |
|---|---|---|---|---|
| normal | 1 | 2588 | S |  |
| normal | 2 | 2588 | S |  |
| normal | 3 | 2588 | S |  |
| sanitizers | 1 | 2588 | S |  |
| sanitizers | 2 | 2588 | S |  |
| sanitizers | 3 | 2588 | S |  |

**Resultado geral: IGUAIS**