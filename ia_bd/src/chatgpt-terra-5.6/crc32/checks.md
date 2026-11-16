# Checks crc32: chatgpt-terra-5.6 vs original

## Escala (instruções do usuário)

| versão | escala 1 | escala 4 | razão 4/1 | custo por repetição |
|---|---|---|---|---|
| original | 3,657,125 | 14,109,576 | 3.86 | 3,484,150 |
| chatgpt-terra-5.6 | 2,033,465 | 7,642,446 | 3.76 | 1,869,660 |

## Variação de entrada (wrap em rand_beebs)

| xor | original | IA | iguais |
|---|---|---|---|
| 0x155 | result=24864 verify=0 | result=11433 verify=1 | N |
| 0x2a3 | result=25425 verify=0 | result=11433 verify=1 | N |
| 0x7f1 | result=8001 verify=0 | result=11433 verify=1 | N |