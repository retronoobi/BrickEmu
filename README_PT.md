# BrickEmu Libretro 1.0.1

BrickEmu é um port do BrickEmuPy criado por azya52. 
O Port foi feito totalmente por IA.

Core libretro nativo para jogos portáteis LCD, desenvolvido para Windows
x86-64 e compatível com a API base do RetroArch 1.7.5 usada pelo EmuVR.

## Jogos suportados

- E23 Plus Mark II 96-in-1
- E88 8-in-1
- E33 2-in-1
- Apollo 18-in-1 B0302
- Apollo 126-in-1 B0202
- Keychain 55-in-1
- GA878
- GA888
- Micon KC32

Vídeo, áudio, controles e save states determinísticos são suportados nos
nove jogos. As máscaras dos segmentos LCD e as ROMs de som necessárias ao
hardware estão incorporadas nos cabeçalhos gerados do core. SVGs, Python,
ImageMagick e o projeto BrickEmuPy completo não são necessários para compilar
ou executar este pacote.

ROMs dos jogos não estão incluídas.

## Desligamento automático

Por padrão, a opção de core `Prevent automatic power-off` fica habilitada.
Ela desperta automaticamente o aparelho quando o firmware tenta entrar no
sono profundo após cerca de cinco a seis minutos sem interação. Isso mantém os
consoles ativos no EmuVR sem alterar vídeo, áudio, controles ou save states.

Defina a opção como `disabled` para restaurar o desligamento automático
original. Quando a prevenção está habilitada, o botão de desligar não mantém o
aparelho desligado permanentemente.

## Dependências para compilação

- Windows 10 ou Windows 11 de 64 bits.
- Visual Studio 2022 Community ou Visual Studio Build Tools 2022.
- Carga de trabalho `Desktop development with C++` / `Desenvolvimento para
  desktop com C++`.
- Compilador MSVC v143 e um Windows 10/11 SDK, instalados pela carga de
  trabalho acima.

Não existem bibliotecas externas, submódulos ou downloads adicionais.

## Como compilar

1. Instale o Visual Studio 2022 ou Build Tools com a carga de trabalho C++.
2. Abra `x64 Native Tools Command Prompt for VS 2022` pelo menu Iniciar.
3. Entre na pasta extraída do pacote:

```bat
cd C:\caminho\para\brickemu-libretro-1.0.1-source
```

4. Execute:

```bat
build_msvc.bat
```

O resultado será criado em:

```text
build\brickemu_libretro.dll
```

Todos os objetos e arquivos auxiliares da compilação também permanecem na
pasta `build`.

## Instalação

Copie `brickemu_libretro.dll` para a pasta de cores do RetroArch/EmuVR. O
arquivo `brickemu_libretro.info` pode ser colocado na pasta `info` do
RetroArch quando os metadados do core forem utilizados.

Carregue diretamente uma das ROMs `.bin` suportadas. O core reconhece cada
jogo pelo conteúdo da ROM, não apenas pelo nome do arquivo.

## Conteúdo do pacote-fonte

- Implementações nativas dos processadores HT943, EM73000, SPL0X e KS56CX2X.
- Interface libretro compatível com RetroArch 1.7.5.
- Máscaras LCD e dados de som pré-gerados e incorporados.
- Script de compilação MSVC, arquivo de exports e metadados do core.

Licença: CC0 1.0 Universal. Consulte `LICENSE`.
