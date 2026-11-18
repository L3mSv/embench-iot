# Checks xgboost: chatgpt-luna-6 vs original

## Escala (instruções do usuário)

| versão | escala 1 | escala 4 | razão 4/1 | custo por repetição |
|---|---|---|---|---|
| original | 4,884,811 | 11,984,277 | 2.45 | 2,366,489 |
| chatgpt-luna-6 | 4,014,160 | 9,808,070 | 2.44 | 1,931,303 |

## Variação de entrada (wrap em rand_beebs)

| xor | original | IA | iguais |
|---|---|---|---|
| 0x155 | result=126 verify=1 (entrada não variou) | result=126 verify=1 | S |
| 0x2a3 | result=126 verify=1 (entrada não variou) | result=126 verify=1 | S |
| 0x7f1 | result=126 verify=1 (entrada não variou) | result=126 verify=1 | S |