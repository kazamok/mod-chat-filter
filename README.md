# AzerothCore 커스텀 모듈: `mod-chat-filter` (채팅 필터 모듈)

## 1. 개요

`mod-chat-filter`는 AzerothCore 서버를 위한 커스텀 모듈로, 게임 내 채팅에서 특정 욕설이나 부적절한 단어 사용을 감지하고 차단하는 기능을 제공합니다. 이 모듈을 통해 서버 운영자는 건전한 채팅 환경을 조성하고 관리할 수 있습니다.

## 2. 주요 기능

*   **욕설/비속어 감지 및 차단:** 설정 파일에 정의된 금지 단어를 포함하는 채팅 메시지를 자동으로 감지하여 다른 플레이어에게 보이지 않도록 차단합니다.
*   **세분화된 로깅:** 금지 단어가 포함된 메시지 사용 시, 발신 플레이어의 정보와 메시지 내용을 특정 로그 파일(`logs/chatfilter/YYYY-MM-DD.log`)에 기록하여 관리자가 추후 검토할 수 있도록 합니다.
*   **설정 가능한 금지 단어 목록:** 설정 파일을 통해 금지 단어 목록을 쉽게 추가, 수정, 삭제할 수 있습니다.
*   **GM 메시지 무시:** GM(게임 마스터)의 채팅 메시지는 필터링 대상에서 제외됩니다.
*   **플레이어 알림:** 메시지가 차단된 플레이어에게는 시스템 메시지를 통해 해당 사실을 알립니다.

## 3. 설치 방법

1.  **모듈 파일 배치:**
    `mod-chat-filter` 폴더 전체를 `azerothcore-wotlk/modules` 디렉토리 안에 복사합니다.
    ```
    azerothcore-wotlk/
    └── modules/
        └── mod-chat-filter/
            ├── conf/
            │   └── mod-chat-filter.conf.dist
            ├── src/
            │   ├── ChatFilter.cpp
            │   ├── ChatFilter.h
            │   └── mod_chat_filter_loader.cpp
            └── include.sh
    ```

2.  **CMake 구성:**
    AzerothCore 빌드 시스템을 재구성(reconfigure)하여 새 모듈을 인식시킵니다.
    *   빌드 폴더(`build/`)로 이동하여 다음 명령을 실행합니다.
        ```bash
        cmake . -DTOOLS=1 -DSCRIPTS=1 -DMODULES=1 # (혹은 기존에 사용하던 cmake 명령어)
        ```
    *   CMake GUI를 사용하는 경우, `Configure` 버튼을 다시 클릭하여 모듈을 활성화합니다.

3.  **컴파일:**
    평소와 같이 AzerothCore 소스 코드를 다시 컴파일합니다.
    ```bash
    cmake --build . --config Release # (혹은 Make, Visual Studio 등으로 빌드)
    ```

4.  **설정 파일 복사:**
    컴파일 완료 후, `azerothcore-wotlk/conf/dist` 폴더 안에 있는 `mod-chat-filter.conf.dist` 파일을 `azerothcore-wotlk/conf` 폴더로 복사하고, 파일명을 `mod-chat-filter.conf`로 변경합니다. (이미 `mod-chat-filter.conf` 파일이 있다면 수정합니다.)

## 4. 설정 (mod-chat-filter.conf)

`azerothcore-wotlk/conf/mod-chat-filter.conf` 파일을 열어 다음 옵션들을 설정할 수 있습니다.

*   **`ChatFilter.Enable`**
    *   **설명:** 채팅 필터 모듈 전체를 활성화하거나 비활성화합니다.
    *   **기본값:** `1` (활성화)
    *   **옵션:** `1` (활성화), `0` (비활성화)

*   **`ChatFilter.Log.Enable`**
    *   **설명:** 필터링된 채팅 메시지의 로깅 기능을 활성화하거나 비활성화합니다. 로그는 `logs/chatfilter/YYYY-MM-DD.log` 경로에 저장됩니다.
    *   **기본값:** `1` (활성화)
    *   **옵션:** `1` (활성화), `0` (비활성화)

*   **`ChatFilter.Badwords`**
    *   **설명:** 채팅에서 필터링할 금지 단어 목록입니다. 쉼표(`,`)로 구분하여 여러 단어를 등록할 수 있습니다. 대소문자를 구분하지 않습니다.
    *   **기본값:** `"욕설,바보,멍청이"` (예시)
    *   **예시:** `"시발,새끼,개새끼,stupid,fuck"`

*   **`ChatFilter.Sender.SeeMessage`**
    *   **설명:** 금지 단어를 사용한 플레이어 본인이 자신의 차단된 메시지를 볼 수 있는지 여부를 설정합니다. `0`으로 설정하면 본인도 볼 수 없으며, `1`로 설정하면 본인에게는 메시지가 표시되지만 다른 플레이어에게는 차단됩니다.
    *   **기본값:** `0` (비활성화, 발신자도 메시지를 볼 수 없음)
    *   **옵션:** `1` (활성화), `0` (비활성화)
    *   **참고:** 현재 구현은 이 설정을 무시하고 발신자에게도 메시지를 보이지 않게 처리합니다. 이 기능이 반드시 필요하다면 추가 개발이 필요합니다.

## 5. 사용 방법

*   **플레이어:** 일반 채팅(말하기, 외치기, 귓속말, 채널 채팅 등)을 할 때, `mod-chat-filter.conf`에 정의된 금지 단어를 사용하면 해당 메시지는 다른 플레이어에게 전달되지 않습니다. 메시지 발신자에게는 "Your message contained inappropriate language and was not sent." 와 같은 시스템 메시지가 표시됩니다.
*   **GM:** GM 계정은 채팅 필터의 영향을 받지 않습니다.
*   **로그 확인:** 필터링된 메시지의 기록은 `logs/chatfilter/` 디렉토리에 날짜별(`YYYY-MM-DD.log`)로 저장됩니다.

## 6. 로그 형식

`logs/chatfilter/YYYY-MM-DD.log` 파일에는 다음과 같은 형식으로 기록됩니다.

```
[HH:MM:SS] Player: <플레이어이름> (Account: <계정이름>, GUID: <플레이어GUID>) said: <원본메시지>
```

---
**개발자:** WiiZZy (예시)
**버전:** 1.0.0 (예시)
**라이센스:** MIT License (예시)
(필요에 따라 개발자 정보 및 라이센스 정보는 수정 가능합니다.)
