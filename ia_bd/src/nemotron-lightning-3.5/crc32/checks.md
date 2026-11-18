# Checks crc32: nemotron-lightning-3.5 vs original

## Escala (instruções do usuário)

| versão | escala 1 | escala 4 | razão 4/1 | custo por repetição |
|---|---|---|---|---|
| original | 3,656,964 | 14,109,414 | 3.86 | 3,484,150 |
| nemotron-lightning-3.5 | 182,430 | 189,059 | 1.04 | 2,210 |

## Variação de entrada (wrap em rand_beebs)

| xor | original | IA | iguais |
|---|---|---|---|
| 0x155 | result=24864 verify=0 | result=11433 verify=1 | N |
| 0x2a3 | result=25425 verify=0 | result=11433 verify=1 | N |
| 0x7f1 | result=8001 verify=0 | result=11433 verify=1 | N |