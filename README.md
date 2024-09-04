# ChattingServer
IOCP Model로 구현한 Chatting Server
로그인 시, DB 계정 정보 조회 및 Redis에 세션 키 저장
-> DB, Redis 작업을 별도의 비동기 스레드에서 처리

1초마다 Monitoring Server로 Monitoring 정보를 전송
 
# NotLogin Version (Single Thread / Multi Thread)
IOCPChatServer_NotLogin_Single

IOCPChatServer_NotLogin_Multi
- Login Server에서의 Login 절차 없이 바로 Chatting Server에 진입하여 통신

# Login Version (Single Thread / Multi Thread)
IOCPChatServer_With_Login_Single

IOCPChatServer_With_Login_Multi
- Login Server에서 Login 절차를 진행한 후, Chatting Server에 진입하여 통신
- Redis DB에 접근하여 클라이언트의 유효성을 판단



