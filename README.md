# grip

C++ 올인원 빌드 툴 & 패키지 매니저.

C++20 기반. 컴파일러 직접 호출(g++/clang++), TOML 설정, 자체 패키지 레지스트리, Linux 우선.

[English](docs/README_en.md)

## 시작하기

```bash
# 새 프로젝트 생성
grip new myproject
cd myproject

# 빌드
grip build

# 실행
grip run

# 정리
grip clean
```

## grip.toml

```toml
[project]
name = "myapp"
version = "0.1.0"
standard = "c++20"
compiler = "g++"

[build]
sources = ["src"]
include = ["include"]
output = "build"
type = "bin"

[dependencies]
fmt = "10.1.0"
spdlog = "1.12.0"

[profile.debug]
opt_level = "0"
debug = true
flags = ["-Wall", "-Wextra"]

[profile.release]
opt_level = "2"
debug = false
flags = ["-Wall", "-DNDEBUG"]

[target.aarch64-linux-gnu]
compiler = "aarch64-linux-gnu-g++"
ar = "aarch64-linux-gnu-ar"
```

## 기능

### 증분 빌드

`#include` 의존성을 추적하고 mtime을 비교해서 변경되지 않은 파일은 건너뜁니다. 수정된 소스만 재컴파일되고, 오브젝트 파일은 따로 링킹됩니다.

### 병렬 빌드

컴파일 단위를 `std::thread::hardware_concurrency()` 기반 스레드 풀에 분배합니다. 독립적인 소스 파일이 동시에 컴파일됩니다.

### Debug / Release 프로필

```bash
grip build              # debug: -O0 -g
grip build --release    # release: -O2 -DNDEBUG
```

출력은 프로필별로 분리: `build/debug/`, `build/release/`.

`grip.toml`의 `[profile.debug]`, `[profile.release]`에서 `opt_level`, `debug`, `flags`를 커스텀할 수 있습니다.

### 패키지 매니저

Cargo 방식의 의존성 관리. 자체 레지스트리 서버에서 패키지를 설치합니다.

```bash
# 특정 버전 설치
grip install fmt@10.1.0

# 최신 버전 설치
grip install fmt
```

패키지는 `grip_modules/{name}/{version}/`에 로컬 저장됩니다. `grip.toml`에 선언된 의존성은 `grip build` 시 자동 설치됩니다.

기능:
- 재귀 의존성 해석
- 헤더 온리 라이브러리 지원 (빈 `source_dir`)
- 패키지별 컴파일러 플래그
- 정적 라이브러리 (`.a`) 생성
- `compile_flags.txt` 자동 생성 (IDE 지원)

### grip.lock

`grip build` 실행 시 `grip.lock` 파일이 생성되어 정확한 의존성 트리를 스냅샷합니다. `grip.lock`이 있으면 레지스트리 대신 lock 파일 기준으로 버전이 고정됩니다.

```toml
[[package]]
name = "fmt"
version = "10.1.0"

[[package]]
name = "spdlog"
version = "1.12.0"
dependencies = ["fmt@10.1.0"]
```

### grip test

```bash
grip test
```

`tests/` 디렉토리의 `.cpp` 파일을 각각 독립 바이너리로 빌드하고 실행합니다. 프로젝트 오브젝트와 의존성이 자동으로 링킹됩니다. exit code 0 = PASS, 그 외 = FAIL.

```
[PASS] test_math
[FAIL] test_parser
Results: 1/2 passed
```

### 크로스 컴파일

```bash
grip build --target aarch64-linux-gnu
```

`grip.toml`의 `[target.<triple>]`에서 타겟 툴체인을 정의합니다. 패키지는 타겟별로 `obj/`, `lib/` 디렉토리가 분리되어 네이티브 빌드와 크로스 빌드가 충돌 없이 공존합니다.

## 프로젝트 구조

```
myproject/
├── grip.toml
├── grip.lock
├── compile_flags.txt
├── include/
│   └── myproject/
├── src/
│   └── main.cpp
├── tests/
│   └── test_basic.cpp
├── grip_modules/
│   └── fmt/
│       └── 10.1.0/
└── build/
    ├── debug/
    └── release/
```

## 레지스트리

grip은 yNet으로 구축된 자체 레지스트리 서버를 사용하며, 패키지 메타데이터를 JSON으로 제공합니다.

**엔드포인트:**
- `GET /packages/:name` — 패키지 정보 및 사용 가능한 버전 목록
- `GET /packages/:name/:version` — 버전별 메타데이터 및 다운로드 URL

**package.json 형식:**
```json
{
    "name": "spdlog",
    "version": "1.12.0",
    "url": "https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.tar.gz",
    "include_dir": "include",
    "source_dir": "src",
    "exclude": [],
    "flags": ["-DSPDLOG_COMPILED_LIB", "-DSPDLOG_FMT_EXTERNAL"],
    "dependencies": ["fmt@10.1.0"]
}
```

## 라이선스

Apache-2.0
