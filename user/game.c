#include "ulib.h"
#include "usyscall.h"
#include "kernel/fctrl.h"
#include "kernel/fs.h"
#include "kernel/monitor.h"
#include "kernel/keyboard.h"

struct monitor_info info;
uint8_t *buf;
float speed = 0.1;
int fps = 60;
int node_size=32;
int nodes_per_line;
int lines_per_screen;

int monitor_fd = -1;


typedef struct state {

    int direction;
    int x, y;
}state_t;

state_t snake[256];
int snake_len = 1;

int food_x, food_y;

void update_state(keyboard_event_t event){
    if(event.type == 1){
        // exit
        if(event.code == 1){
            exit(0);
        }

        if(event.code == 103){
            snake[0].direction = 0;
        }
        if(event.code == 106){
            snake[0].direction = 1;
        }
        if(event.code == 108){
            snake[0].direction = 2;
        }
        if(event.code == 105){
            snake[0].direction = 3;
        }
    }
}

void init_snake(){

    // init snake
    snake[0].x = 10;
    snake[0].y = 10;
    snake[0].direction = 0;

    snake[1].x = 10;
    snake[1].y = 11;
    snake[1].direction = 0;

    snake_len = 2;

}
// 线性同余生成器参数
#define LCG_A 1664525
#define LCG_C 1013904223
#define LCG_M 0xFFFFFFFF

// 伪随机数生成器的状态
static uint32_t lcg_state = 1;

// 初始化伪随机数生成器的种子
void srand(uint32_t seed) {
    lcg_state = seed;
}

// 生成伪随机数
uint32_t rand() {
    lcg_state = (LCG_A * lcg_state + LCG_C) & LCG_M;
    return lcg_state;
}

// 示例：生成一个范围在 [min, max] 之间的伪随机数
int rand_range(int min, int max) {
    return min + (rand() % (max - min + 1));
}


void random_food(){

    int x = rand_range(0, nodes_per_line-1);
    int y = rand_range(0, lines_per_screen-1);
    while(1){
        int can = 1;
        for(int i = 0; i < snake_len; i++){
            if(snake[i].x == x && snake[i].y == y){
                can=0;
                break;
            }
        }
        if(can){
            food_x = x;
            food_y = y;
            break;
        }
    }


}

void update_snake(){

    int last_x = snake[snake_len-1].x;
    int last_y = snake[snake_len-1].y;
    int last_direction = snake[snake_len-1].direction;

    for(int i = snake_len-1; i > 0; i--){
        snake[i].x = snake[i-1].x;
        snake[i].y = snake[i-1].y;
        snake[i].direction = snake[i-1].direction;
    }
    
    if(snake[0].direction == 0){
        if(snake[0].y == 0){
            snake[0].y = nodes_per_line-1;
        }else{
            snake[0].y--;
        }
    }
    if(snake[0].direction == 1){
        if(snake[0].x == nodes_per_line-1){
            snake[0].x = 0;
        }else{
            snake[0].x++;
        }
    }
    if(snake[0].direction == 2){
        if(snake[0].y == nodes_per_line-1){
            snake[0].y = 0;
        }else{
            snake[0].y++;
        }
    }
    if(snake[0].direction == 3){
        if(snake[0].x == 0){
            snake[0].x = nodes_per_line-1;
        }else{
            snake[0].x--;
        }
    }
    if(snake[0].x == food_x && snake[0].y == food_y){
        snake_len++;
        snake[snake_len-1].x = last_x;
        snake[snake_len-1].y = last_y;
        snake[snake_len-1].direction = last_direction;
        random_food();
    }
    
    
    
}

void draw_snake(){
    for(int i = 0; i < snake_len; i++){
        for(int j = 0; j < node_size; j++){
            for(int k = 0; k < node_size; k++){
                buf[(snake[i].y*node_size+j)*info.width*4 + (snake[i].x*node_size+k)*4] = 0;
                buf[(snake[i].y*node_size+j)*info.width*4 + (snake[i].x*node_size+k)*4+1] = 0;
                buf[(snake[i].y*node_size+j)*info.width*4 + (snake[i].x*node_size+k)*4+2] = 0;
                buf[(snake[i].y*node_size+j)*info.width*4 + (snake[i].x*node_size+k)*4+3] = 255;
            }
        }
    }
}

void draw_food(){

    for(int j = 0; j < node_size; j++){
        for(int k = 0; k < node_size; k++){
            buf[(food_y*node_size+j)*info.width*4 + (food_x*node_size+k)*4] = 255;
            buf[(food_y*node_size+j)*info.width*4 + (food_x*node_size+k)*4+1] = 0;
            buf[(food_y*node_size+j)*info.width*4 + (food_x*node_size+k)*4+2] = 0;
            buf[(food_y*node_size+j)*info.width*4 + (food_x*node_size+k)*4+3] = 255;
        }
    }

}

void draw_screen(){
    for(int i = 0; i < info.height; i++){
        for(int j = 0; j < info.width; j++){
            buf[i*info.width*4 + j*4] = 255;
            buf[i*info.width*4 + j*4+1] = 255;
            buf[i*info.width*4 + j*4+2] = 255;
            buf[i*info.width*4 + j*4+3] = 255;
        }
    }
    draw_snake();
    draw_food();
    write(monitor_fd, (char*)buf, info.width*info.height*4);
}


int main(){

    monitor_fd = open("/dev/monitor0", O_RDWR);
    if(monitor_fd < 0){
        fprintf(2, "cannot open monitor\n");
        exit(1);
    }

    if(ioctl(monitor_fd, MONITOR_GET_INFO,(uint64_t) &info) < 0){
        fprintf(2, "cannot get monitor info\n");
        exit(1);
    }

    printf("width: %d, height: %d\n", info.width, info.height);

    nodes_per_line = info.width/node_size;
    lines_per_screen = info.height/node_size;

    buf = (uint8_t*)malloc(info.width*info.height*4);

    for(int i = 0; i < info.width*info.height*4; i++){
        buf[i] = 255;
    }

    write(monitor_fd, (char*)buf, info.width*info.height*4);
    
    int keyboard_fd = open("/dev/keyboard", O_RDONLY);
    
    if(keyboard_fd < 0){
        fprintf(2, "cannot open keyboard\n");
        exit(1);
    }

    init_snake();
    random_food();

    keyboard_event_t event[128];
    while(1){
        
        
        int begin = get_timer_ticks();
        while(get_timer_ticks() - begin < 2);
        int n = read(keyboard_fd, (char*)&event, sizeof(event));
        if(n !=0 ){
            update_state(event[n/sizeof(keyboard_event_t)-1]);
        }
    
        update_snake();
        draw_screen();
            
        
        
        
        

    }


    close(monitor_fd);

    free(buf);
}