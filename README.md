# PT2399 UGen

  <a href="https://github.com/schollz/pt2399/releases/latest"><img src="https://img.shields.io/github/v/release/schollz/pt2399" alt="Version"></a>
  <a href="https://github.com/schollz/pt2399/actions/workflows/release.yml"><img src="https://github.com/schollz/pt2399/actions/workflows/ci.yml/badge.svg" alt="Build Status"></a>
  <a href="https://github.com/sponsors/schollz"><img alt="GitHub Sponsors" src="https://img.shields.io/github/sponsors/schollz"></a>

This the [One-Bit Delay](https://onebitdelay.com/) plugin I developed for DAWs, ported to SuperCollider and tuned to emulate the PT2399 Delay chip, release for free.

Read more about it here: https://onebitdelay.com

Support the development of this and all my work buy [sponsoring me](https://github.com/sponsors/schollz) or [purchase the DAW plugin](https://onebitdelay.com/#buy)!

## Build

```bash
make build
```

## Install

```bash
make install
```

Installs to:

- `~/.local/share/SuperCollider/Extensions/PT2399UGen/plugins/PT2399UGens.so`
- `~/.local/share/SuperCollider/Extensions/PT2399UGen/Classes/PT2399.sc`
- `~/.local/share/SuperCollider/Extensions/PT2399UGen/HelpSource/Classes/PT2399.schelp`

## Test

```bash
make test
```

