#!/usr/bin/env bash

# 判断 Mach-O 架构列表是否包含指定发布架构。
zz_macos_arch_list_contains() {
  local architectures=${1-}
  local expected=${2-}
  case $expected in
    arm64|x86_64) ;;
    *) return 1 ;;
  esac
  case " $architectures " in
    *" $expected "*) return 0 ;;
    *) return 1 ;;
  esac
}

# 判断第一方 Mach-O 是否严格为指定的单一发布架构。
zz_macos_arch_list_is_exact() {
  local architectures=${1-}
  local expected=${2-}
  case $expected in
    arm64|x86_64) ;;
    *) return 1 ;;
  esac
  [[ $architectures == "$expected" ]]
}
