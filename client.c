#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#define SOCKET_PATH "convert"

void show(int connfd, int count) {  //사용여부 출력
    for (int i = 0; i < count; i++) {
        int number;
        int available;
        read(connfd, &number, sizeof(number));   //사물함 번호 읽기
        read(connfd, &available, sizeof(available));  //사용여부 읽기
        printf("사물함 %d: %s\n", number, available ? "사용 가능" : "사용 중");
    }
}

int main() {
    int sockfd;
    struct sockaddr_un serverUNIXaddr;
    int count;
    int lock_num;
    char password[10];
    char product[100];

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("소켓 생성 실패");
        exit(EXIT_FAILURE);
    }

    memset(&serverUNIXaddr, 0, sizeof(struct sockaddr_un));  //서버 주소 초기화
    serverUNIXaddr.sun_family = AF_UNIX;
    strncpy(serverUNIXaddr.sun_path, SOCKET_PATH, sizeof(serverUNIXaddr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&serverUNIXaddr, sizeof(struct sockaddr_un)) < 0) {  //서버연결
        perror("연결 실패");
        exit(EXIT_FAILURE);
    }

    read(sockfd, &count, sizeof(count));   //사물함 생성개수 확인
    int choice;
    while (1) {
        printf("\n\n사물함----키오스크\n\n");
        printf("1. 사물함 사용여부 확인기\n");
        printf("2. 사물함 등록\n");
        printf("3. 물품 보관\n");
        printf("4. 물품 회수\n");
        printf("5. 관리자 호출(비밀번호 잃어버렸을 때)\n");
        printf("6. 종료\n");
        printf("선택: ");
        scanf("%d", &choice);

        write(sockfd, &choice, sizeof(choice));  //선택한 옵션 쓰기

        switch (choice) {
        case 1:
            show(sockfd, count);
            break;
        case 2: {
            printf("사용하실 사물함 번호: ");
            scanf("%d", &lock_num);
            printf("비밀번호: ");
            scanf("%s", password);
            printf("\n등록되었습니다.\n");
            write(sockfd, &lock_num, sizeof(lock_num));   //사물함 번호 쓰기
            write(sockfd, password, sizeof(password));    //비밀번호 쓰기
            break;
        }
        case 3: {
            printf("등록되어 있는 번호: ");
            scanf("%d", &lock_num);
            printf("비밀번호: ");
            scanf("%s", password);
            write(sockfd, &lock_num, sizeof(lock_num));   //번호 쓰기
            write(sockfd, password, sizeof(password));  //product 쓰기

            int result;
            read(sockfd, &result, sizeof(result));
            if (result) { //등록 안됨 or 비밀번호 불일치
                printf("\n\n<<<<등록된 사물함이 아니거나, 비밀번호가 일치하지 않습니다>>>>\n");
            }
            else {  
                printf("보관할 물건: ");
                scanf("%s", product);
                write(sockfd, product, sizeof(product));
                printf("물품 %s가 보관되었습니다\n", product);
            }
            break;
        }
        case 4: {
            printf("사용중인 번호: ");
            scanf("%d", &lock_num);
            printf("비밀번호: ");
            scanf("%s", password);
            write(sockfd, &lock_num, sizeof(lock_num));
            write(sockfd, password, sizeof(password));
            int correct;
            read(sockfd, &correct, sizeof(correct));   //correct한지 읽기
            if (correct) {
                printf("물품 %s가 회수되었습니다\n", product);
                printf("\n사용해주셔서 감사합니다.\n");
            }
            else {
                printf("\n<<<<등록된 사물함이 아니거나, 비밀번호가 일치하지 않습니다>>>>\n");
            }
            break;
        }
        case 5: {
            printf("\n관리자 호출\n");
            printf("\n사물함 번호: ");
            scanf("%d", &lock_num);
            printf("관리자가 비밀번호를 초기화 하였습니다.(비밀번호 0000)\n");
            write(sockfd, &lock_num, sizeof(lock_num));
            break;
        }
        case 6:
            close(sockfd);
            exit(EXIT_SUCCESS);
        default:
            printf("\n해당하는 옵션이 없습니다.\n");
            break;
        }
    }

    return 0;
}

