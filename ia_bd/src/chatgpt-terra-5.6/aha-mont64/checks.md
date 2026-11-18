# Checks aha-mont64: chatgpt-terra-5.6 vs original

## Escala (instruções do usuário)

| versão | escala 1 | escala 4 | razão 4/1 | custo por repetição |
|---|---|---|---|---|
| original | 2,580,447 | 9,838,864 | 3.81 | 2,419,472 |
| chatgpt-terra-5.6 | 2,291,734 | 8,680,726 | 3.79 | 2,129,664 |

## Variação de entrada (wrap em rand_beebs)

| xor | original | IA | iguais |
|---|---|---|---|
| 0x155 | result=0 verify=1 (entrada não variou) | result=0 verify=1 | S |
| 0x2a3 | result=0 verify=1 (entrada não variou) | result=0 verify=1 | S |
| 0x7f1 | result=0 verify=1 (entrada não variou) | result=0 verify=1 | S |