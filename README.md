# VmWatcher
이벤트 기반 실시간 메모리 프로파일러

## 주요 특징
+ 커널 빌트인 모듈 형식의 Netlink-Socket 드라이버 구현
+ brk, mmap, munmap, page fault 주요 시스템 콜 소스 수정
+ 실시간 분석
+ 로그 파일 제공
+ 리눅스 IPC 프로그래밍

## 구조
### Kernel
+ netlink socket 드라이버 구현
+ 시스템 콜 발생 시 유저단으로 소켓 통신

### User
+ 멀티프로세싱 (Proc1, Proc2)
+ Proc1
  + 커널에서 소켓 송신 대기
  + 정보를 pipe 통해서 Proc2로 전달
+ Proc2
  + Proc1에서 전달받은 데이터 활용, /proc/[PID]탐색 후 status 정보 파싱
  + UI출력 및 로깅
 

## 실행 화면
<img width="587" height="134" alt="스크린샷 2025-11-02 오후 11 25 09" src="https://github.com/user-attachments/assets/cbd7aeee-bd10-4bbc-80e8-af1bb17f2b50" />

<img width="583" height="359" alt="스크린샷 2025-11-02 오후 11 26 48" src="https://github.com/user-attachments/assets/f19b2c84-0759-40dd-9ffa-b239a4b13d75" />
