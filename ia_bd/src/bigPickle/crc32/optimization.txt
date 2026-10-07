opencode (Big Pickle) - CRC32 benchmark optimization
==================================================

Arquivo original (intocado): src/crc32/crc_32.c
Arquivo otimizado:           ia_bd/src/opencode/crc32/crc_32.c

Flags de build (inalteradas, as do scons/baseline):
  gcc -O2 -fdata-sections -ffunction-sections -DWARMUP_HEAT=1 -DGLOBAL_SCALE_FACTOR=1
  link: -O2 -Wl,-gc-sections  com bd/support/main.o, beebsc.o, config/boardsupport.o


Mudancas aplicadas
------------------

1. Estreitamento de tipos: DWORD/UNS_32_BITS passam de "unsigned long" (64 bits
   em LP64) para uint32_t (via <stdint.h>).
   Por que ajuda: o registro de CRC tem exatamente 32 bits.  No original cada
   entrada da tabela ocupava 8 bytes (0x800 = 2 KiB em vez de 1 KiB) e todo
   shift/xor do acumulador era uma operacao de 64 bits.  Com 32 bits a tabela
   encolhe, o working set do L1 encolhe e o codigo fica mais curto
   (menos prefixos REX / operandos de 64 bits).  O resultado e bit-identico
   por inducao: tab < 2^32 e crc0 = 0xFFFFFFFF, logo o acumulador nunca passa
   de 32 bits.

2. CRC "slicing-by-4" (byte-slicing / slicing-by-N) - 4 bytes por passo.
   Por que ajuda (principal ganho de ciclos): no codigo original cada byte
   consome uma busca seriada na tabela encadeada no registrador:
       crc = tab[(crc ^ b) & 0xff] ^ (crc >> 8)
   O caminho critico e, por byte: xor -> mascarar -> carga (~4-5 ciclos de L1)
   -> xor -> proximo byte, ou seja ~8 ciclos/byte e o processador fica
   bloqueado nessa cadeia.

   Como crc_32_tab[0] == 0, a funcao Z(x) = tab[x & 0xff] ^ (x >> 8) satisfaz
   Z(0) == 0, logo Z e *linear* em GF(2): Z(x ^ y) == Z(x) ^ Z(y).  Daí a
   formula de 4 fatias (a mesma que a zlib usa no modo BYFOUR):
       crc' = T4[c0^b0] ^ T3[c1^b1] ^ T2[c2^b2] ^ T1[c3^b3]
   com Tk[b] = Z^k(b) e c0..c3 os 4 bytes do registrador de entrada.  As quatro
   buscas ficam mutuamente independentes: o unico elo carried do laco passa a
   ser a reducao XOR final, nao quatro cargas encadeadas.  Cadeia atual:
   extrair bytes (1) ^ xor do indice (1) ^ carga (5) ^ arvore XOR (2) ~= 9
   ciclos a cada 4 bytes => ~2,25 ciclos/byte.

   A tabela foi gerada por
       Tk[b] = crc_32_tab[T(k-1)[b] & 0xff] ^ (T(k-1)[b] >> 8)
   portanto a fatia 0 e *byte a byte* a tabela original de 256 entradas
   (0x00000000, 0x77073096, 0xee0e612c, ...) e cada valor das demais fatias e
   exatamente o valor que o codigo original produziria.  Polinomio CRC
   0xedb88320 inalterado.

3. Versao inlineavel e bit-identica de rand_beebs() (LCG) no proprio arquivo.
   Por que ajuda: rand_beebs() vive em outra unidade de traducao e, sem LTO,
   o compilador nao pode inlinar: eram 1024 chamadas por crc32pseudo(), cada
   uma com call/ret e, pior, com leitura *e escrita* da variavel estatica
   `seed` em memoria a cada passo (imul $imm,mem / mov %rax,mem).  A
   aritmetica mantida e exatamente a de support/beebsc.c:
       seed = (seed * 1103515245UL + 12345UL) & ((1UL << 31) - 1UL);
   Como a mascara descarta tudo acima do bit 31, o resultado e identico tanto
   com "unsigned long" de 64 bits (LP64) quanto com o wrap de 32 bits - ou
   seja, identico em qualquer alvo.  Tambem se guarda apenas o byte que o CRC
   realmente consome ((BYTE)rand_beebs(), isto e, bits 16..23 do estado).

4. Quebra da cadeia de dependencias do LCG com duas faixas (stride 2).
   Por que ajuda: o LCG e uma recorrencia puramente serial (cada estado
   depende do anterior: imul 3 + add 1 + and 1 = ~5 ciclos), o que sozinho
   imporia um piso de ~5 ciclos/byte mesmo com o slicing do item 2.  Compondo a
   recorrencia com ela mesma:
       s(n+2) = (A*A)*s(n) + (A*C + C)   (mod 2^31)
   o gerador roda em duas faixas independentes (faixa A = estados s(1), s(3),
   ... para os bytes de indice par; faixa B = s(2), s(4), ... para os de
   indice impar), cada uma com metade do comprimento da cadeia.  Constantes:
   LCG_MULT2 = 1117952617 = A^2 mod 2^31, LCG_INC2 = 1406932606 =
   (A*C + C) mod 2^31.  Isso NAO precomputa nada: as mesmas 1024 etapas do
   gerador sao executadas, apenas reordenadas em duas correntes - e o piso
   passa a ~2,5 ciclos/byte, abaixo do ~2,25 do CRC.

5. Desenrolamento de 8 bytes por iteracao (2 fatias de 4 bytes), 128 iteracoes.
   Por que ajuda: 1024 e multiplo de 8, entao nao ha laco de limpeza; o
   overhead de laco (add/cmp/jmp) cai de 1024 para 128 execucoes por
   crc32pseudo() e o compilador ganha instrucoes independentes para
   escalonar entre a cadeia do LCG e a do CRC.


Interface do benchmark (inalterada)
------------------------------------
- crc32pseudo(), initialise_benchmark(), warm_caches(), benchmark(),
  benchmark_body(), verify_benchmark(): mesmos nomes, mesmas assinaturas,
  mesmo papel.  verify_benchmark() continua exigindo 11433.
- benchmark_body() continua issuing srand_beebs (0) antes de cada
  crc32pseudo(), e a semente do gerador inline (s(1)) vem desse reset.
- LOCAL_SCALE_FACTOR 170, tamanho da entrada 1024 bytes, polinomio 0xedb88320
  e tabela original: inalterados.
- Sem alocacao dinamica, sem bibliotecas externas, sem assembly inline, sem
  codigo especifico de arquitetura, sem pre-computacao de resultado e sem
  pular trabalho.


Verificacao
-----------
- gcc -O2 ... -DWARMUP_HEAT=1 -DGLOBAL_SCALE_FACTOR=1; ./crc32 -> exit 0
  (verify_benchmark passou, resultado 11433).
- gcc -O2 ... -DWARMUP_HEAT=4 -DGLOBAL_SCALE_FACTOR=4; ./alt -> exit 0.
- Comparacao direta do valor completo de crc32pseudo() contra o binario do
  baseline (mesmo harness, 3 chamadas com srand_beebs(0) antes):
      baseline 0x65842ca9 == otimizado 0x65842ca9  (identico nas 3 chamadas).
- Identidade slicing-by-4 conferida numericamente antes de escrever o C:
  20.000 pares (registro, 4 bytes) aleatorios + varredura exaustiva de
  65.536 valores do registrador com varios padroes de bytes.
- -Wall -Wextra: nenhum aviso novo (so os 3 avisos pre-existentes do
  arquivo original: `res` e `i` nao usados, `r` possivelmente nao
  inicializada).


Tamanho (`size` no binario ligado) - comparacao com o baseline
---------------------------------------------------------------
                 text    data   bss    data+bss (RAM)
  baseline      4013     520    16        536
  otimizado     6413     520    16        536   (+2400)

  Origem do aumento de texto: a tabela de feedback passa de 0x800 para
  0x1000 bytes (2048 -> 4096) porque agora sao 4 fatias de 256 entradas de
  32 bits (o mesmo footprint de 4 KiB que a zlib usa no slicing-by-4),
  enquanto o codigo em si encolhe.  data/bss (RAM) nao mudam: os +2 KiB sao
  somente .rodata (read-only).


Instrucoes no laco quente (contagem via objdump)
-----------------------------------------------
  baseline : 8 insns no corpo do laco + 6 em rand_beebs() = 14 insn/byte
  otimizado: 87 insns por 8 bytes  = 10,9 insn/byte        (-22%)

Cadeia de dependencias (analise, sem medicao):
  baseline : ~8 ciclos/byte  (limite: 4 cargas encadeadas no registrador)
  otimizado: ~2,5-3 ciclos/byte (limite: cadeia do LCG com 2 faixas; a
             cadeia do CRC e ~2,25 ciclos/byte e sobrepoe-se a ela)
  => reducao de ciclos estimada de ~2,7x a ~3x, com ~22% menos instrucoes
     (o IPC tambem sobe, de ~1,8 para ~4).