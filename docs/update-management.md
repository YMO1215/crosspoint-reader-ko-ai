# CrossPoint Korean AI Update Management

이 문서는 `YMO1215/crosspoint-reader-ko-ai` 업데이트를 반복 가능하게 관리하기 위한 공식 절차입니다.

## 6하 원칙

| 항목 | 내용 |
| --- | --- |
| 누가 | `YMO1215/crosspoint-reader-ko-ai` 유지보수자 |
| 언제 | upstream `crosspoint-reader` 안정 태그가 나온 뒤, 한국어 포크 기준 기능 검토가 끝났을 때 |
| 어디서 | GitHub 저장소 `YMO1215/crosspoint-reader-ko-ai`와 GitHub Releases |
| 무엇을 | CrossPoint Reader 본체, 한국어 UI, `.epdfont` SD 글꼴, CJK 렌더링 안정화, OTA/SD 업데이트 산출물 |
| 왜 | 최신 upstream 기능을 받으면서도 `crosspoint-reader-ko` 작성자가 안정화한 한국어 화면/폰트/줄바꿈 품질을 유지하기 위해 |
| 어떻게 | Git remote/tag 기반으로 upstream과 한국어 포크를 추적하고, 변경 내용을 커밋/태그/릴리스 노트로 문서화한다 |

## 기준 저장소

- 최신 코드 기준: `https://github.com/crosspoint-reader/crosspoint-reader`
- 한국어 안정화 기준: `https://github.com/crosspoint-reader-ko/crosspoint-reader-ko`
- 배포 대상: `https://github.com/YMO1215/crosspoint-reader-ko-ai`

## 한국어 적용 정책

- upstream `.cpfont` 체계 대신 한국어 포크에서 안정화한 `.epdfont` 체계를 유지한다.
- 읽기 본문 글꼴과 UI 시스템 글꼴은 각각 SD `.epdfont`로 선택할 수 있어야 한다.
- UI 시스템 글꼴에 없는 글자는 Pretendard로 glyph 단위 fallback한다.
- KoPub Batang과 Pretendard 내장 폰트는 한국어 기본 표시 품질의 기준이므로 유지한다.
- `characterWrap`, `paragraphIndent`, 한국어 line compression, justify gap 완화는 리더 품질 기준 기능으로 유지한다.
- CJK 대형 글꼴의 interval table on-demand 탐색과 XTC 메모리 안정화는 회귀 금지 항목이다.

## 버전 관리

태그는 OTA 파서가 안정적으로 비교할 수 있도록 `v` 없이 작성한다.

예:

```bash
git tag 1.4.0-ko.0
git push origin 1.4.0-ko.0
```

버전 번호 규칙:

- `1.4.0-ko.0`: upstream `1.4.0` 기반 첫 한국어 AI 릴리스
- `1.4.0-ko.1`: 같은 upstream 기반 후속 수정
- `1.4.1-ko.0`: upstream `1.4.1` 기반 첫 한국어 AI 릴리스

## 업데이트 절차

1. 원격 기준을 갱신한다.

```bash
git fetch origin --tags
git fetch upstream --tags
git fetch ko --tags
```

2. 한국어 포크의 안정화 변경을 먼저 반영한다.

```bash
git merge ko/release/korean
```

3. upstream 안정 태그를 반영한다.

```bash
git merge 1.4.0
```

4. 충돌 해결 원칙을 적용한다.

- `.epdfont` 관련 충돌은 한국어 포크 쪽을 우선한다.
- `.cpfont`, `SdCardFont*`, `FontInstaller`, `FontsPage.html`은 한국어 릴리스에서는 기본적으로 제외한다.
- 1.4.0의 EPUB, RTL, 북마크, 이미지, 키보드, KOReader Sync 개선은 가능한 한 유지한다.
- 다국어 YAML은 릴리스 크기 예산 때문에 `english.yaml`, `korean.yaml`만 유지한다.

5. 문서와 릴리스 노트를 작성한다.

- `docs/release-notes/<tag>.md`를 반드시 작성한다.
- 변경 이유, 적용 범위, 업데이트 방법, 검증 항목을 6하 원칙에 맞춰 적는다.

6. 로컬 검증을 수행한다.

```bash
pio run
pio run -e gh_release
```

7. 커밋과 태그를 push한다.

```bash
git push origin HEAD
git push origin 1.4.0-ko.0
```

태그 push 후 `.github/workflows/release.yml`이 GitHub Release를 만들고 다음 파일을 첨부한다.

- 통합 플래시 이미지 `CrossPoint-<tag>.bin`
- OTA/SD용 `firmware.bin`
- `bootloader.bin`
- `partitions.bin`
- 디버그용 `firmware.elf`, `firmware.map`

## 업데이트 방법

사용자 업데이트 경로:

- OTA: Wi-Fi 연결 후 설정 -> 시스템 -> 업데이트
- SD: `CrossPoint-<tag>-firmware.bin` 또는 `firmware.bin`을 SD 카드에 복사 후 설정 -> 시스템 -> SD카드 펌웨어 업데이트
- 수동 플래시: Release의 bootloader/partitions/firmware 파일을 받아 ESP32-C3 flash offset에 맞춰 기록

## 릴리스 전 체크리스트

- [ ] 기본 UI가 한국어로 표시된다.
- [ ] 한국어 메뉴에 깨진 글자나 검은 박스가 없다.
- [ ] KoPub Batang 본문과 Pretendard UI fallback이 정상 동작한다.
- [ ] SD `.epdfont` 읽기 글꼴과 시스템 글꼴을 각각 선택할 수 있다.
- [ ] 대형 CJK `.epdfont` 선택 시 메모리 오류가 없다.
- [ ] EPUB justified 한국어 문단에서 과도한 단어 간격이 없다.
- [ ] TXT 앞/뒤 페이지 재구성이 정상이다.
- [ ] XTC가 대형 SD 글꼴 사용 중에도 열린다.
- [ ] `gh_release`의 `firmware.bin`이 6,553,600 bytes 이하인지 확인했다.
- [ ] OTA URL이 `YMO1215/crosspoint-reader-ko-ai`를 가리킨다.
- [ ] `docs/release-notes/<tag>.md`가 존재한다.
