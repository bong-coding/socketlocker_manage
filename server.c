#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/stat.h>

#define DEFAULT_PROTOCOL 0
#define SOCKET_PATH "convert"

typedef struct locker {
    int number;
    char password[10];
    bool use;
    char product[100];
} loc;


struct flock global_lock;  //락

void reset(loc* loc, int count) {   // 사물함 초기화// 사물함 초기화
    for (int i = 0; i < count; i++) {
        loc[i].number = i;
        loc[i].use = true;   // 초기 상태는 사용 가능한 상태로 설정
        strncpy(loc[i].product, "비어있음", sizeof(loc[i].product) - 1);
        loc[i].product[sizeof(loc[i].product) - 1] = '\0';   // 문자열 끝에 널 추가  
        strncpy(loc[i].password, "설정안됨", sizeof(loc[i].password) - 1);
        loc[i].password[sizeof(loc[i].password) - 1] = '\0';   // 문자열 끝에 널 추가
    }
}

void save_all(loc* loc, int count) {   //사물함 정보 저장
    int fd = open("lock_information", O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) {
        perror("파일 열기 실패");
        exit(EXIT_FAILURE);
    }
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);   // 파일 잠금
    if (write(fd, loc, count * sizeof(loc)) == -1) {
        perror("파일 쓰기 실패");
        exit(EXIT_FAILURE);
    }
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);    // 파일 잠금 해제
    close(fd);
}

void load_all(loc* loc, int count) {     //사물함 정보 파일에 로드
    int fd = open("lock_information", O_RDONLY);
    if (fd == -1) {
        perror("파일 열기 실패");
        exit(EXIT_FAILURE);
    }
    struct flock lock;
    lock.l_type = F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    fcntl(fd, F_SETLKW, &lock);    // 파일 잠금
    if (read(fd, loc, count * sizeof(loc)) == -1) {
        perror("파일 읽기 실패");
        exit(EXIT_FAILURE);
    }
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);    // 파일 잠금 해제
    close(fd);
}

void loc_updatefile(loc* locker, int index) {    //특정 사물함 정보 파일에 업데이트
    int fd = open("lock_information", O_WRONLY | O_CREAT, 0666);
    if (fd == -1) {
        perror("파일 열기 실패");
        exit(EXIT_FAILURE);
    }
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = index * sizeof(loc);          //사물함 위치로 이동
    lock.l_len = sizeof(loc);
    fcntl(fd, F_SETLKW, &lock);
    if (lseek(fd, index * sizeof(loc), SEEK_SET) == -1) {    //파일 위치로 이동
        perror("파일 위치 이동 실패");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if (write(fd, locker, sizeof(loc)) == -1) {
        perror("파일 쓰기 실패");
        close(fd);
        exit(EXIT_FAILURE);
    }
    lock.l_type = F_UNLCK;
    fcntl(fd, F_SETLK, &lock);                          // 파일 잠금 해제
    close(fd);
}

void show(loc* loc, int count, int connfd) {                //사용자에게 사물함 현재 사용여부를 보여줌
    printf("========================================\n");
    for (int i = 0; i < count; i++) {
        printf("사물함 %d: %s, 내용물: %s\n", i, loc[i].use ? "사용 가능" : "사용 중", loc[i].product); //사용가능 여부를 정수로 변환

        int available = loc[i].use ? 1 : 0;
        write(connfd, &loc[i].number, sizeof(loc[i].number));   //사물함 번호 쓰기
        write(connfd, &available, sizeof(available));            //사용자에게 사용가능 여부 쓰기
    }
}

int main() {
    int listenfd, connfd, lock_num;
    struct sockaddr_un serverUNIXaddr, clientUNIXaddr;
    socklen_t client_len;
    char password[10], product[100];

    listenfd = socket(AF_UNIX, SOCK_STREAM, DEFAULT_PROTOCOL);
    if (listenfd < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    memset(&serverUNIXaddr, 0, sizeof(struct sockaddr_un));   //바인딩
    serverUNIXaddr.sun_family = AF_UNIX;
    strncpy(serverUNIXaddr.sun_path, SOCKET_PATH, sizeof(serverUNIXaddr.sun_path) - 1);
    unlink(SOCKET_PATH);

    if (bind(listenfd, (struct sockaddr*)&serverUNIXaddr, sizeof(struct sockaddr_un)) < 0) {
        perror("바인딩 실패");
        exit(EXIT_FAILURE);
    }

    listen(listenfd, 5);    //대기 큐

    printf("================================================\n");
    printf("생성하실 사물함의 개수를 입력해주세요 : ");
    int count;
    scanf("%d", &count);

    loc* locker_array = (loc*)malloc(count * sizeof(loc));    //사물함 수
    if (locker_array == NULL) {
        perror("메모리 할당 실패");
        exit(EXIT_FAILURE);
    }
    reset(locker_array, count);     //사물함 초기화
    save_all(locker_array, count);   //초기화 된 정보 파일에 저장

    //초기화
    global_lock.l_type = F_WRLCK;
    global_lock.l_whence = SEEK_SET;
    global_lock.l_start = 0;
    global_lock.l_len = 0;

    while (1) {
        client_len = sizeof(struct sockaddr_un);
        connfd = accept(listenfd, (struct sockaddr*)&clientUNIXaddr, &client_len);  //클라이언트 접속
        if (connfd < 0) {
            perror("연결 수락 실패");
            continue;
        }

        if (fork() == 0) {    //자식 프로세스 생성
            close(listenfd);

            int lockfd = open("lockfile", O_CREAT | O_RDWR, 0666);
            if (lockfd == -1) {
                perror("파일 잠금 실패");
                exit(EXIT_FAILURE);
            }

            fcntl(lockfd, F_SETLKW, &global_lock);    // 사용자가 작업동안 잠금

            load_all(locker_array, count);          //사물함 정보 
            write(connfd, &count, sizeof(count));  //사물함 개수 쓰기

            int choice;
            while (read(connfd, &choice, sizeof(choice)) > 0) {
                switch (choice) {
                case 1:
                    show(locker_array, count, connfd);
                    break;
                case 2:
                    read(connfd, &lock_num, sizeof(lock_num));  //사용자 번호 읽기
                    read(connfd, password, sizeof(password));   //사용자 비밀번호 읽기
                    if (lock_num >= 0 && lock_num < count && locker_array[lock_num].use) {
                        strncpy(locker_array[lock_num].password, password, sizeof(locker_array[lock_num].password) - 1);
                        locker_array[lock_num].password[sizeof(locker_array[lock_num].password) - 1] = '\0';
                        locker_array[lock_num].use = false;  //사용중으로 변경
                        loc_updatefile(&locker_array[lock_num], lock_num);  // 사물함 업데이트
                    }
                    break;
                case 3:
                    read(connfd, &lock_num, sizeof(lock_num));
                    read(connfd, password, sizeof(password));

                    int result = 0;
                    if (lock_num >= 0 && lock_num < count && !locker_array[lock_num].use &&
                        strcmp(locker_array[lock_num].password, password) == 0) {  //비밀번호 일치 or 등록된 사물함이면 결과 전송 
                        write(connfd, &result, sizeof(result));
                        read(connfd, product, sizeof(product));
                        strncpy(locker_array[lock_num].product, product, sizeof(locker_array[lock_num].product) - 1);
                        locker_array[lock_num].product[sizeof(locker_array[lock_num].product) - 1] = '\0';
                        loc_updatefile(&locker_array[lock_num], lock_num);
                    }
                    else {
                        result = 1;  //확인 실패
                        write(connfd, &result, sizeof(result));
                    }
                    break;
                case 4:
                    read(connfd, &lock_num, sizeof(lock_num));
                    read(connfd, password, sizeof(password));
                    int correct = 0;   // 비밀번호 체크
                    if (lock_num >= 0 && lock_num < count &&
                        strcmp(locker_array[lock_num].password, password) == 0) {
                        strncpy(locker_array[lock_num].product, "비어있음", sizeof(locker_array[lock_num].product) - 1);
                        locker_array[lock_num].product[sizeof(locker_array[lock_num].product) - 1] = '\0';
                        locker_array[lock_num].use = true;   //사용가능으로 변경
                        loc_updatefile(&locker_array[lock_num], lock_num); //업데이트
                        correct = 1; // 비밀번호 동일
                    }
                    write(connfd, &correct, sizeof(correct));  // 결과 여부 쓰기
                    break;
                case 5:
                    printf("\n관리자가 호출되었습니다.\n");
                    read(connfd, &lock_num, sizeof(lock_num));
                    if (lock_num >= 0 && lock_num < count) {
                        strncpy(locker_array[lock_num].password, "0000", sizeof(locker_array[lock_num].password) - 1);  //비밀번호 0000으로 리셋
                        locker_array[lock_num].password[sizeof(locker_array[lock_num].password) - 1] = '\0';
                        loc_updatefile(&locker_array[lock_num], lock_num);
                        printf("\n사물함 %d번의 비밀번호가 초기화되었습니다.\n", lock_num);
                    }
                    break;
                case 6:
                    close(connfd);
                    close(lockfd);
                    exit(EXIT_SUCCESS);
                default:
                    break;
                }
            }
            close(connfd);
            // 사용자가 작업이 끝날 때 잠금해제 
            global_lock.l_type = F_UNLCK;
            fcntl(lockfd, F_SETLK, &global_lock);
            close(lockfd);
            exit(0);
        }
        else {
            close(connfd); //새로운 사용자 대기
        }
    }

    return 0;
}

