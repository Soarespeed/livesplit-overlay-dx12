# Compilar a partir do código-fonte

Este addon é um único arquivo C++ compilado em uma DLL de addon do ReShade
(`.addon64`). Você não precisa do Visual Studio completo — só do compilador de
linha de comando do MSVC.

## 1. Instalar o compilador

Baixe o **Build Tools for Visual Studio** (gratuito) da Microsoft:
https://visualstudio.microsoft.com/downloads/ (procure por "Build Tools for Visual
Studio", na seção "Tools for Visual Studio").

Durante a instalação, marque a carga de trabalho **"Desenvolvimento para desktop
com C++"**. Isso instala o `cl.exe` (compilador MSVC) e o Windows SDK.

## 2. Obter os headers

O addon precisa de dois conjuntos de headers que **não** estão neste repositório,
porque têm licença própria:

- **Headers de addon do ReShade** — do SDK do ReShade:
  https://github.com/crosire/reshade (a pasta `include`), na versão do ReShade que
  você quer usar. Este projeto foi compilado com a API do ReShade versão 20.
- **Headers do Dear ImGui** — versão **1.92.5 (branch docking)**:
  https://github.com/ocornut/imgui/tree/docking

Junte todos esses arquivos `.h` / `.hpp` numa pasta só, por exemplo `include\`,
ao lado do código-fonte.

Estrutura esperada:

```
seu-projeto\
  src\
    livesplit_overlay.cpp
  include\
    reshade.hpp
    reshade_api_device.hpp
    ... (resto dos headers do ReShade)
    imgui.h
    imgui_internal.h
    ... (resto dos headers do ImGui)
```

## 3. Compilar

Abra o **"x64 Native Tools Command Prompt for VS"** (instalado junto com o Build
Tools), entre na pasta do projeto com `cd` e rode:

```bat
cl /nologo /std:c++17 /EHsc /O2 /LD /DNDEBUG /Iinclude /Iinclude\imgui src\livesplit_overlay.cpp /Fe:livesplit_overlay.addon64 /link user32.lib gdi32.lib
```

O que cada parte faz:

- `/std:c++17` — o código usa C++17.
- `/O2` — otimiza para velocidade.
- `/LD` — gera uma DLL (o addon é uma DLL).
- `/Iinclude` — onde encontrar os headers do ReShade e do ImGui.
- `/Fe:livesplit_overlay.addon64` — nome do arquivo final. Addons do ReShade usam
  a extensão `.addon64` para jogos 64 bits.
- `/link user32.lib gdi32.lib` — o addon usa a API GDI do Windows
  (`PrintWindow`, `CreateDIBSection`, etc.) para capturar a janela do LiveSplit.

Se compilar sem erros, você terá o `livesplit_overlay.addon64` na pasta.

## 4. Testar

Copie o `livesplit_overlay.addon64` para a pasta de um jogo que já tenha o ReShade
com suporte a addons, ao lado do `ReShade.ini`. Abra o jogo, abra o menu do ReShade
(Home), e a seção `livesplit_overlay` aparece na aba **Add-ons**.

## Observações

- Só o `livesplit_overlay.cpp` é o addon. Os outros arquivos de código que
  existiram no histórico (`_cmdlist`, `_text`) eram experimentos e não são a versão
  
  publicada.
- Se o `cl` não for reconhecido, você não está no "x64 Native Tools Command
  Prompt". Abra esse prompt específico pelo menu Iniciar — um cmd comum não tem o
  compilador no PATH.
