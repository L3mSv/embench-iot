opencode - CRC32 benchmark optimization
========================================

Arquivo original: src/crc32/crc_32.c (intocado)
Arquivo otimizado: ia_bd/src/opencode/crc32/crc_32.c

Tecnicas aplicadas
------------------

1. Inlining do gerador pseudo-aleatorio rand_beebs() dentro de crc32pseudo().
   O gerador e um LCG simples (support/beebsc.c):
     seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed >> 16.
   Como benchmark_body() sempre chama srand_beebs(0) imediatamente antes de
   crc32pseudo(), o estado pode partir de um `uint32_t seed = 0` local e gerar
   exatamente a mesma sequencia de 1024 bytes.
   Por que ajuda: elimina 1024 chamadas de funcao (call/ret, trafego de pilha
   e barreiras de escalonamento) por chamada de crc32pseudo() e permite ao
   compilador intercalar a aritmetica do LCG com os loads da tabela CRC,
   escondendo a latencia load-to-use. O trabalho e identico (1024 passos do
   LCG + 1024 atualizacoes CRC).

2. Desenrolamento (loop unrolling) por fator 4 do laco de 1024 iteracoes.
   1024 e multiplo de 4, sem codigo de limpeza.
   Por que ajuda: reduz o overhead do laco (comparacao, incremento e desvio)
   de 1024 para 256 iteracoes por chamada e da ao compilador mais instrucoes
   para escalonar em torno da cadeia serial do CRC.

3. Estreitamento de tipo da tabela e do acumulador para 32 bits exatos.
   Tabela: `static const UNS_32_BITS` (unsigned long: 64 bits em LP64, 2 KiB)
   -> `static const uint32_t` (1 KiB, valores identicos). Acumulador e seed
   locais passam a `uint32_t` (antes `DWORD` = unsigned long de 64 bits).
   Por que ajuda: todos os valores cabem em 32 bits e o CRC nunca excede
   32 bits por inducao (tab < 2^32, crc0 = 0xFFFFFFFF), logo o resultado e
   bit-identico nos 32 bits baixos; usa aritmetica de 32 bits (menos
   instrucoes/prefixos REX em x86-64), metade do trafego da tabela e melhor
   localidade de cache. Efeito colateral positivo: o texto cai ~856 bytes
   em relacao ao baseline.

Corretude
---------
- Mesma tabela, mesmos valores, mesma macro UPDC32, mesmo valor inicial
  0xFFFFFFFF e mesmo complemento final nos 32 bits baixos.
- Mesmas constantes de entrada e de tamanho: 1024 bytes, LOCAL_SCALE_FACTOR
  170, polinomio 0xedb88320, constantes do LCG inalteradas.
- benchmark_body() mantido (inclusive srand_beebs(0) por iteracao), assim como
  as assinaturas de crc32pseudo, initialise_benchmark, warm_caches,
  benchmark e verify_benchmark.
- Sem pre-computacao, sem saida embutida, sem pular trabalho; sem malloc,
  sem libs externas, sem asm inline, sem codigo especifico de arquitetura.

Verificacao (exit 0 = verify_benchmark passou, resultado 11433):
- gcc -O2 -fdata-sections -ffunction-sections -DWARMUP_HEAT=1
  -DGLOBAL_SCALE_FACTOR=1, link com bd/support/main.o, beebsc.o,
  boardsupport.o: exit 0.

Tamanho (`size` no binario ligado, gcc -O2):
- baseline: text=4013 data=520 bss=16
- este:      text=3157 data=520 bss=16  (-856 bytes de texto, -21%; dados/RAM iguais)
