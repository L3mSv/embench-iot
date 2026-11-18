# Checks xgboost: nemotron-lightning-3.5 vs original

## Escala (instruções do usuário)

| versão | escala 1 | escala 4 | razão 4/1 | custo por repetição |
|---|---|---|---|---|
| original | 4,884,812 | 11,984,278 | 2.45 | 2,366,489 |
| nemotron-lightning-3.5 | 3,776,490 | 9,213,474 | 2.44 | 1,812,328 |

## Variação de entrada (wrap em rand_beebs)

| xor | original | IA | iguais |
|---|---|---|---|
| 0x155 | result=126 verify=1 (entrada não variou) | result=10 verify=1 | N |
| 0x2a3 | result=126 verify=1 (entrada não variou) | result=10 verify=1 | N |
| 0x7f1 | result=126 verify=1 (entrada não variou) | result=10 verify=1 | N |