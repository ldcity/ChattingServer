# ChattingServer
IOCP Model로 구현한 Chatting Server
1초마다 Monitoring Server로 Monitoring 정보를 전송합니다.
 
* NotLogin Version
- Login Server에서의 Login 절차 없이 바로 Chatting Server에 진입하여 통신 합니다.

* Login Version
- Login Server에서 Login 절차를 진행한 후, Chatting Server에 진입하여 통신 합니다.
- Redis DB에 비동기적으로 접근하여 계정의 유효성을 판단합니다.

IOCPChatServer_NotLogin_Single
IOCPChatServer_With_Login_Single
- Contents Logic이 Single Thread에서 동작합니다.

IOCPChatServer_NotLogin_Multi
IOCPChatServer_With_Login_Multi
- Contents Logic이 Multi Thread에서 동작합니다.


