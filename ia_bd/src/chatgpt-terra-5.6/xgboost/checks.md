# Checks xgboost: chatgpt-terra-5.6 vs original

## Escala (instruções do usuário)

| versão | escala 1 | escala 4 | razão 4/1 | custo por repetição |
|---|---|---|---|---|
| original | 4,884,817 | 11,984,277 | 2.45 | 2,366,487 |
| chatgpt-terra-5.6 | 1,012,656 | 2,304,307 | 2.28 | 430,550 |

## Variação de entrada (wrap em rand_beebs)

| xor | original | IA | iguais |
|---|---|---|---|
| 0x155 | result=126 verify=1 (entrada não variou) | result=126 verify=1 | S |
| 0x2a3 | result=126 verify=1 (entrada não variou) | result=126 verify=1 | S |
| 0x7f1 | result=126 verify=1 (entrada não variou) | result=126 verify=1 | S |