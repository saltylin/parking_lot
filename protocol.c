#include "proxy.h"
#include "list.h"
#include "queue.h"
#include "protocol.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <sys/signal.h>
#include <string.h>

static int total_park_num;
static int remaining_park_num;
int nodetype;

static int self_pid;
static list pid_list;
static queue car_queue;
static CLIENT *clnt;
static int original_list_length;

static pthread_mutex_t pid_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t car_queue_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool is_joining_flag;
static int join_wait_pid;
static int join_allow_num;
static int join_confirm_num;

static int park_response_num;
static bool is_allow_park_flag;

static bool join_switch_flag;
static bool park_switch_flag;

static bool ring_flag;

static pthread_mutex_t ring_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t join_switch_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t park_switch_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t remaining_park_num_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t is_joining_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t is_allow_park_flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t parking_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t join_allow_num_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t join_confirm_num_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t park_response_num_mutex = PTHREAD_MUTEX_INITIALIZER;

void protocol_init() {
    char **str_list;
    int i, j, k;
    struct sigaction join_action, park_action;
    is_joining_flag = true;
    join_allow_num = 0;
    join_confirm_num = 0;
    self_pid = getpid();
    pid_list = create_list(10);
    car_queue = create_queue();
    clnt = clnt_create("127.0.0.1", PROXY, PROXY_VERSION, "tcp");
    if (clnt == (CLIENT*) NULL) {
        clnt_pcreateerror("127.0.0.1");
        exit(1);
    }
    rpc_call_point:
    str_list = get_pid_list_1(&self_pid, clnt);
    if (str_list == (char **) NULL) {
        clnt_perror(clnt, "call failed");
    }
    if (strlen(*str_list) == 0) {/* This client is the first client */
        is_joining_flag = false;
        total_park_num = *comfirm_1(&self_pid, clnt);
        remaining_park_num = total_park_num;
    }
    else if ((*str_list)[0] == '#') {
        sleep(1);
        goto rpc_call_point;
    } else {
        /* convert the pids in the str_list into pid_list */
        i = 0;
        do {
            list_add(pid_list, atoi(*str_list + i));
            for (j = i; (*str_list)[j] != '\0' && (*str_list)[j] != ' '; ++j)
                ;
            i = j + 1;
        } while ((*str_list)[j] != '\0');
    }
    original_list_length = list_length(pid_list);
    join_action.sa_sigaction = join_response;
    park_action.sa_sigaction = park_response;
    join_action.sa_flags = SA_SIGINFO;
    park_action.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &join_action, NULL);
    sigaction(SIGUSR2, &park_action, NULL);
    ring_flag = true;
    if (original_list_length != 0) {
        ring_flag = false;
        pthread_mutex_lock(&parking_mutex);/* Before joining into the group, the node cannot park any car */
        join_request();
    }
}

void run() {
    const double pr = 0.5;
    int random_number, arrive_time;
    srand(time(NULL));
    while(1) {
        random_number = rand() % 1000;
        if ((double)random_number / 1000 < pr) {
            arrive_time = time(0);
            pthread_mutex_lock(&car_queue_mutex);
            enqueue(car_queue, arrive_time);
            pthread_mutex_unlock(&car_queue_mutex);
        }
        sleep(2);
    }
}

void join_request() {
    int i;
    bool flag = true;
    union sigval value;
    value.sival_int = JOIN_REQUEST;
    join_switch_flag = true;
    for (i = 0; i != original_list_length + 1; ++i) {
        while(flag) {
            pthread_mutex_lock(&join_switch_flag_mutex);
            flag = !join_switch_flag;
            pthread_mutex_unlock(&join_switch_flag_mutex);
            sleep(1);
        }
        if (i == original_list_length)
            join_confirm();
        else {
            flag = true;
            join_switch_flag = false;
            sigqueue(list_get(pid_list, i), SIGUSR1, value);
        }
    }
}

void join_confirm() {
    int i;
    union sigval value;
    bool flag = true;
    join_switch_flag = true;
    value.sival_int = JOIN_CONFIRM;
    for (i = 0; i != original_list_length; ++i) {
        while(flag) {
            pthread_mutex_lock(&join_switch_flag_mutex);
            flag = !join_switch_flag;
            pthread_mutex_unlock(&join_switch_flag_mutex);
            sleep(1);
        }
        flag = true;
        join_switch_flag = false;
        sigqueue(list_get(pid_list, i), SIGUSR1, value);
    }
    total_park_num = *comfirm_1(&self_pid, clnt);
    printf("该节点成功加入\n");
    clnt_destroy(clnt);
}

void join_response(int signo, siginfo_t *info, void *ctx) {
    int from_pid;
    int sig_info;
    union sigval value;
    from_pid = info->si_pid;
    sig_info = info->si_int;
    if (sig_info == JOIN_REQUEST) {
        pthread_mutex_lock(&parking_mutex);
        join_wait_pid = from_pid;
        value.sival_int = JOIN_ALLOW;
        sigqueue(from_pid, SIGUSR1, value);
    } else if (sig_info == JOIN_CONFIRM) {
        value.sival_int = remaining_park_num;
        sigqueue(from_pid, SIGUSR1, value);
        pthread_mutex_lock(&pid_list_mutex);
        list_add(pid_list, from_pid);
        pthread_mutex_unlock(&pid_list_mutex);
        pthread_mutex_unlock(&parking_mutex);
    } else if (sig_info == JOIN_ALLOW) {
        pthread_mutex_lock(&join_allow_num_mutex);
        ++join_allow_num;
        pthread_mutex_unlock(&join_allow_num_mutex);
        pthread_mutex_lock(&join_switch_flag_mutex);
        join_switch_flag = true;
        pthread_mutex_unlock(&join_switch_flag_mutex);
    } else if (sig_info >= 0) {
        pthread_mutex_lock(&remaining_park_num_mutex);
        remaining_park_num = sig_info;
        pthread_mutex_unlock(&remaining_park_num_mutex);
        pthread_mutex_lock(&join_confirm_num_mutex);
        ++join_confirm_num;
        if (join_confirm_num == original_list_length) {
            pthread_mutex_lock(&is_joining_flag_mutex);
            is_joining_flag = false;
            pthread_mutex_unlock(&is_joining_flag_mutex);
            pthread_mutex_unlock(&parking_mutex);
        }
        pthread_mutex_unlock(&join_confirm_num_mutex);
        pthread_mutex_lock(&join_switch_flag_mutex);
        join_switch_flag = true;
        pthread_mutex_unlock(&join_switch_flag_mutex);
    }
}

void *start_request(void *arg) {
    while(1) {
        if (nodetype == ENTRANCE && remaining_park_num == 0
            || nodetype == EXIT && remaining_park_num == total_park_num) {
            pthread_mutex_lock(&car_queue_mutex);
            queue_clear(car_queue);
            pthread_mutex_unlock(&car_queue_mutex);
        }
        if (!ring_flag) {
            sleep(1);
            continue;
        }
        park_request();
        sleep(1);
    }
}

void park_request() {
    int i, j, len;
    int arrive_time;
    union sigval value;
    bool flag;
    char *str;
    time_t tmp_time;
    pthread_mutex_lock(&parking_mutex);
    len = list_length(pid_list);
    pthread_mutex_lock(&car_queue_mutex);
    if (queue_isempty(car_queue) || nodetype == ENTRANCE && remaining_park_num == 0
        || nodetype == EXIT && remaining_park_num == total_park_num) {
        if (len != 0) {
            ring_flag = false;
            j = original_list_length % len;
            value.sival_int = RING;
            sigqueue(list_get(pid_list, j), SIGUSR2, value);
        }
        pthread_mutex_unlock(&car_queue_mutex);
        pthread_mutex_unlock(&parking_mutex);
        return;
    }
    arrive_time = queue_first(car_queue);
    tmp_time = arrive_time;
    pthread_mutex_unlock(&car_queue_mutex);
    value.sival_int = arrive_time;
    if (len == 0) {
        dequeue(car_queue);
        if (nodetype == ENTRANCE) {
            --remaining_park_num;
            str = ctime(&tmp_time);
            str[strlen(str)-1] = '\0';
            printf("进入一辆车（到达时间：%s），剩余空停车位：%d\n", str, remaining_park_num);
        }
        else {
            ++remaining_park_num;
            printf("出来一辆车，剩余空停车位：%d\n", remaining_park_num);
        }
        pthread_mutex_unlock(&parking_mutex);
        return;
    }
    is_allow_park_flag = true;
    flag = true;
    park_switch_flag = true;
    for (i = 0; i != len + 1; ++i) {
        while(flag) {
            pthread_mutex_lock(&park_switch_flag_mutex);
            flag = !park_switch_flag;
            pthread_mutex_unlock(&park_switch_flag_mutex);
            sleep(0.2);
        }
        if (i == len) {
            if (is_allow_park_flag) {
                park_confirm(tmp_time);
            }
            else {
                ring_flag = false;
                j = original_list_length % len;
                value.sival_int = RING;
                sigqueue(list_get(pid_list, j), SIGUSR2, value);
                pthread_mutex_unlock(&parking_mutex);
            }
        }
        else {
            flag = true;
            park_switch_flag = false;
            sigqueue(list_get(pid_list, i), SIGUSR2, value);
        }
    }
}

void park_confirm(time_t tmp_time) {
    int i, len;
    union sigval value;
    bool flag;
    char *str;
    value.sival_int = nodetype == ENTRANCE? PARK_ADD_CONFIRM: PARK_SUB_CONFIRM;
    len = list_length(pid_list);
    if (nodetype == ENTRANCE) {
        --remaining_park_num;
        str = ctime(&tmp_time);
        str[strlen(str)-1] = '\0';
        printf("进入一辆车（到达时间：%s），剩余空停车位：%d\n", str, remaining_park_num);
    }
    else {
        ++remaining_park_num;
        printf("出来一辆车，剩余空停车位：%d\n", remaining_park_num);
    }
    flag = true;
    park_switch_flag = true;
    for (i = 0; i != len + 1; ++i) {
        while(flag) {
            pthread_mutex_lock(&park_switch_flag_mutex);
            flag = !park_switch_flag;
            pthread_mutex_unlock(&park_switch_flag_mutex);
            sleep(0.2);
        }
        if (i == len) {
            pthread_mutex_lock(&car_queue_mutex);
            dequeue(car_queue);
            pthread_mutex_unlock(&car_queue_mutex);
            pthread_mutex_unlock(&parking_mutex);
        }
        else {
            flag = true;
            park_switch_flag = false;
            sigqueue(list_get(pid_list, i), SIGUSR2, value);
        }
    }
}

void park_response(int signo, siginfo_t *info, void *ctx) {
    int from_pid, info_value;
    int self_first_time;
    union sigval value;
    bool flag;
    from_pid = info->si_pid;
    info_value = info->si_int;
    if (info_value > 0) {
        pthread_mutex_lock(&is_joining_flag_mutex);
        flag = is_joining_flag;
        pthread_mutex_unlock(&is_joining_flag_mutex);
        if (flag) {
            value.sival_int = PARK_DISALLOW;
            sigqueue(from_pid, SIGUSR2, value);
            return;
        }
        pthread_mutex_lock(&car_queue_mutex);
        flag = queue_isempty(car_queue);
        if(!flag)
            self_first_time = queue_first(car_queue);
        pthread_mutex_unlock(&car_queue_mutex);
        if (flag || info_value < self_first_time || info_value == self_first_time && from_pid < self_pid)
            value.sival_int = PARK_ALLOW;
        else
            value.sival_int = PARK_DISALLOW;
        sigqueue(from_pid, SIGUSR2, value);
    } else if (info_value == PARK_ADD_CONFIRM) {
        --remaining_park_num;
        value.sival_int = PARK_CONFIRM;
        sigqueue(from_pid, SIGUSR2, value);
        printf("剩余空停车位：%d\n", remaining_park_num);
    } else if (info_value == PARK_SUB_CONFIRM) {
        ++remaining_park_num;
        value.sival_int = PARK_CONFIRM;
        sigqueue(from_pid, SIGUSR2, value);
        printf("剩余空停车位：%d\n", remaining_park_num);
    } else if (info_value == PARK_ALLOW || info_value == PARK_DISALLOW) {
        if (info_value == PARK_DISALLOW) {
            pthread_mutex_lock(&is_allow_park_flag_mutex);
            is_allow_park_flag = false;
            pthread_mutex_unlock(&is_allow_park_flag_mutex);
        }
        pthread_mutex_lock(&park_switch_flag_mutex);
        park_switch_flag = true;
        pthread_mutex_unlock(&park_switch_flag_mutex);
    } else if (info_value == PARK_CONFIRM) {
        pthread_mutex_lock(&park_switch_flag_mutex);
        park_switch_flag = true;
        pthread_mutex_unlock(&park_switch_flag_mutex);
    } else if (info_value == RING) {
        pthread_mutex_lock(&ring_flag_mutex);
        ring_flag = true;
        pthread_mutex_unlock(&ring_flag_mutex);
    }
}
