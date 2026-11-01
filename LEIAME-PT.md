# LiveSplit Overlay para ReShade (DirectX 12)

Sobrepõe a janela do LiveSplit dentro do jogo, em tela cheia, usando o ReShade.
Funciona em jogos DirectX 12 (como Resident Evil 4 Remake), que o addon original
não suportava. Inclui chroma key para remover o fundo e deixar só o texto.

## Compatibilidade

Funciona com DirectX 9, DirectX 11, DirectX 12 e Vulkan. O suporte ao DirectX 12 foi adicionado sobre o addon original.

## Instalação

1. Tenha o ReShade instalado no jogo, com suporte a addons.
2. Copie `livesplit_overlay.addon64` para a pasta do jogo, no mesmo lugar do
   `ReShade.ini` (normalmente ao lado do executável do jogo).
3. Abra o jogo e o LiveSplit. Não minimize o LiveSplit: se ele ficar minimizado,
   a captura para.
4. No LiveSplit, desligue a opção **Always on Top**. Com ela ligada, a janela real
   do LiveSplit pode aparecer sobreposta ao overlay dentro do jogo até você dar
   alt+tab.
5. Abra o menu do ReShade (tecla Home) e vá na aba **Add-ons**, seção
   `livesplit_overlay`, para ajustar as opções.

## Opções

- **Show LiveSplit** — liga e desliga o overlay.

- **Vertical/Horizontal Alignment** — posiciona o overlay na tela. O primeiro
  controle move na horizontal, o segundo (logo abaixo, sem rótulo) move na vertical.
  0 = esquerda/topo, 1 = direita/baixo.

- **Vertical/Horizontal Offsets** — deslocamento fino em pixels, somado ao
  alinhamento. Útil para encaixar na posição exata.

- **Scale** — tamanho do overlay. 1.00 = tamanho original da janela do LiveSplit
.
- **Overlay opacity** — transparência geral do overlay. 1.00 = opaco, 0.00 = invisível.

- **Capture rate (Hz)** — quantas vezes por segundo o addon lê a janela do LiveSplit.
  30 é fluido. Esse valor roda numa thread separada e quase não afeta o FPS.

- **Upload rate (Hz) - lower = more FPS** — quantas vezes por segundo a imagem é
  enviada para a placa de vídeo. **É o controle que mais afeta o FPS.** Valores
  menores = mais FPS, mas o número do timer atualiza em saltos. As trocas de split
  são sempre enviadas na hora, então só a contagem do timer fica menos fluida.
  Experimente 15, 10 ou 8 até achar o equilíbrio.

- **Chroma key (remove background)** — remove o fundo do LiveSplit, deixando só o
  texto e as barras, parecido com o chroma key do OBS.

- **Background color** — a cor que vira transparente. Ajuste para a cor de fundo do
  seu layout do LiveSplit (preto, por padrão).

- **Key tolerance** — o quanto a cor pode variar e ainda ser removida. Se sobrar
  fundo, aumente. Se começar a sumir texto, diminua.

- **Reset position** — volta alinhamento, deslocamento e escala ao padrão.

## Desempenho e FPS

Em jogos **DirectX 12**, o overlay pode custar entre **1 e 4 FPS**, dependendo da
placa de vídeo e do jogo. Isso acontece porque o DirectX 12 exige que a imagem do
overlay seja enviada para a GPU de forma sincronizada a cada atualização, e esse
envio tem um custo que não dá para eliminar por completo nessa configuração.

Para reduzir ao máximo:

- Baixe o **Upload rate (Hz)** para 10 ou 8.
- Se mesmo assim quiser o FPS mais estável, limite o FPS do jogo a 2 ou 3 abaixo
  da sua meta (por exemplo, 117 em vez de 120), por um limitador externo. Com essa
  folga, o custo do envio não derruba o frame abaixo do alvo.

## Observações para speedrun

Este addon apenas **lê e exibe** a janela do LiveSplit. Ele não altera o timer, os
splits, nem os arquivos do jogo ou do LiveSplit. Ainda assim, confira as regras do
site onde você envia suas runs antes de usar qualquer overlay.
