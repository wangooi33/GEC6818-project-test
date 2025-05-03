//都可借助链表实现，方便切换界面和管理

//arm-linux-gcc main.c -I./include -L. -L./lib -lfont -lfont_old -ljpeg -lpthread -lm

//快捷键：ALT+SHIFT+A:快捷加注释
//		  CTRL+[  or  CTRL+]:增加或减少缩进

//   ./表示当前目录  ..返回上一级路径

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <pthread.h>

#include "fonto.h"
#include "font.h"
#include "jpeglib.h"

//登录按钮坐标
#define login_min_X 126
#define login_max_X 354
#define login_min_y 380
#define login_max_y 431
//注册按钮坐标
#define regist_min_X 490
#define regist_max_X 723
#define regist_min_y 380
#define regist_max_y 431
//登录和注册页面的返回欢迎页坐标
#define return_min_x 385
#define return_max_x 472
#define return_min_y 361
#define return_max_y 422


int lcd_fd=-1,ts_fd=-1;//文件描述符
int *p=NULL; //野指针，可能会指向不可访问区域，导致程序崩溃

int x,y;

int current_page;//现在处于的界面
enum UI_STATE { 
	LOGIN_PAGE, //登录
	REGIST_PAGE,//注册
	MAIN_PAGE, //主界面（各项功能）
	CARD_PAGE,//充电卡
	SCANTOPAY_PAGE,//扫码支付
	INFO_PAGE,//账户信息
	CHARGINGMODE_PAGE,//charging mode,充电模式选择（快 or 慢）
	CHARGINGINFO1_PAGE,//充电监控
	CHARGINGINFO2_PAGE,//设备信息
	EXIT_PAGE//充电结束，欢迎下次使用
};
int current_page = LOGIN_PAGE;


//打开LCD、触摸屏、图片
void init(void)
{
	//打开lcd
	lcd_fd = open("/dev/fb0",O_RDWR);
	if(lcd_fd < 0)   //打开失败
	{
		perror("open lcd failed");
		return;
	}
	//进行内存映射
	p = mmap(NULL,800*480*4,PROT_READ | PROT_WRITE,MAP_SHARED,lcd_fd,0);
	if(p == NULL)
	{
		perror("mmap failed");
		return;
	}

	//路径：/dev/input/event0
	ts_fd = open("/dev/input/event0",O_RDWR);
	if (ts_fd < 0)
	{
		perror("open ts_fd failed");//打印错误信息
		return;
	}
}

////解除映射，关闭lcd和触摸屏
void uninit(void)
{
	close(lcd_fd);
	close(ts_fd);
}

void show_bmp(int x0,int y0,char *bmp_name)
{
	//打开bmp
	int bmp_fd = open(bmp_name,O_RDWR);
	if(bmp_fd < 0)   //打开失败
	{
		perror("open bmp failed");
		return;
	}
	
	//读取前54个字节，获取图片的宽度和高度
	char head[54];
	read(bmp_fd,head,54);
	//宽度：第18~21字节
	int width = head[18] | head[19] << 8 | head[20] << 16 | head[21] << 24;
	//高度：第22~25字节
	int height = head[22] | head[23] << 8 | head[24] << 16 | head[25] << 24;
	//printf("w = %d,h = %d\n",width,height);
	//判断图片越界
	if(width+x0 > 800 || height+y0 > 480)
	{
		printf("out of range\n");
		return;
	}
	
	//读取图片数据
	char bmp_buf[width*height*3];
	read(bmp_fd,bmp_buf,width*height*3);
	
	int x,y,color,i=0;
	char r,g,b;
	for(y=0;y<height;y++)
	{
		for(x=0;x<width;x++)
		{
			b = bmp_buf[i++];
			g = bmp_buf[i++];
			r = bmp_buf[i++];
			color = b | g << 8 | r << 16;
			*(p+800*(height-1-y+y0)+x+x0) = color;  
		}
	}
	close(bmp_fd);
}

//获取x，y坐标
void gets_pos(int *x,int *y)
{
	//读取触摸屏数据，获取坐标
	struct input_event buf;
	while(1)
	{
		read(ts_fd,&buf,sizeof(buf));
		if(buf.type == EV_ABS) //触摸屏事件
		{
			if(buf.code == ABS_X) //x轴
			{
				*x = buf.value*800/1024;
			}
			if(buf.code == ABS_Y) //Y轴
			{
				*y = buf.value*480/600;
			}
		}
		//判断手指松开
		if(buf.type == EV_KEY && buf.code == BTN_TOUCH && buf.value == 0) break;
	}
}


/*****************************************************************/
void Num_lock(void);  // 加上这个声明
//int return_welcome=0;

int arr_Account[4];
int arr_Password[4];

char Character_account;
char Character_password='*';

int account=1;//=1,输入账号
int password=0;//=0,不输入密码
int account_x=120,account_y=245;
int password_x=120,password_y=305;
int input_i=0;//账号计数
int input_j=0;//密码计数

void main_page(void);  // 加在所有函数使用之前（如 login 之前）
void *show_time(void *arg);
void *show_id(void *arg);

void login(void)
{
    show_bmp(0,0,"welcome.bmp");
	sleep(1);
    input_i=0;
	input_j=0;
	//return_welcome=0;
	account_x=120;
	account_y=245;
	show_bmp(0,0,"login.bmp");
    while(1)
    {
        gets_pos(&x,&y);
        printf("(%d,%d)\n",x,y);
      //点击框内
        if(x>100 && x<330)
        {
            if(y>250 && y<300)//输入账号
            {
                account=1;
                password=0;
                input_i=0;
				account_x=120;
				account_y=245;
            }
            if(y>314 && y<365)
            {
                account=0;
				password=1;
				input_j=0;
				password_x=120;
				password_y=305;
            }
        }
        Num_lock();
        if (x>25&&x<355&&y>400&&y<444)
		{
			for(int i=0;i<4;i++)
				{
					printf("%d",arr_Account[i]);
				}
				printf("\n");
				for(int i=0;i<4;i++)
				{
					printf("%d",arr_Password[i]);
				}
			if(arr_Account[0]==1&&arr_Account[1]==2&&arr_Account[2]==3&&arr_Account[3]==4 &&
			arr_Password[0]==1&&arr_Password[1]==2&&arr_Password[2]==3&&arr_Password[3]==4)
			{
				current_page = MAIN_PAGE;
				return;
			}
		}
		if(x>400 && x<475 && y>350 && y<425)
		{
			current_page = LOGIN_PAGE;
			return;
		}
    }
}

void regist(void)
{
    show_bmp(0,0,"welcome.bmp");
    input_i=0;
	input_j=0;
	account_x=120;
	account_y=245;
	//return_welcome=0;
	show_bmp(0,0,"login.bmp");
    while(1)
	{
		gets_pos(&x,&y);
		printf("(%d,%d)\n",x,y);
		if(x>100 && x<330)
		{
			if(y>250&&y<300)//输入账户
			{
				account=1;
				password=0;
				account_x=120;
				account_y=245;
			}
			if(y>314 && y<365)//输入密码
			{
				account=0;
				password=1;
				password_x=120;
				password_y=305;
			}
		}
		Num_lock();
		//if(arr_Account)
		{
			if (x>25&&x<355&&y>400&&y<444)
			{
				printf("注册账户：%s\n注册密码：%s\n",arr_Account,arr_Password);
				if(arr_Account[1]!=0 &&arr_Password[1]!=0) 
                {
					current_page=MAIN_PAGE;
					return;
				}
			}
		}
		if(x>400 && x<475 && y>350 && y<425)
		{
			current_page = LOGIN_PAGE;
			return;
		}
	}
}

//键盘
void Num_lock(void)
{
	if(y>120 && y<208)//1 2 3
	{
		if(x>389 && x<503)
		{
			if(account)
			{
				arr_Account[input_i]=1;
				//user[input_i].account[input_i]=1
				Character_account='1';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=1;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}
		else if(x>503 && x<630)
		{
			if(account)
			{
				arr_Account[input_i]=2;
				Character_account='2';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2);
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=2;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}
		else if(x>630 && x<762)
		{
			if(account)
			{
				arr_Account[input_i]=3;
				Character_account='3';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=3;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}
	}
	else if(y>208 && y<275)//4 5 6
	{
		if(x>389 && x<503)
		{
			if(account)
			{
				arr_Account[input_i]=4;
				Character_account='4';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=4;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}

		}
		else if(x>503 && x<630)
		{
			if(account)
			{
				arr_Account[input_i]=5;
				Character_account='5';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=5;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
			
		}
		else if(x>630 && x<762)
		{
			if(account)
			{
				arr_Account[input_i]=6;
				Character_account='6';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=6;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_i++;
			}
		}
	}
	else if(y>275 && y<360)//7 8 9
	{
		if(x>389 && x<503)
		{
			if(account)
			{
				arr_Account[input_i]=7;
				Character_account='7';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=7;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}
		else if(x>503 && x<630)
		{
			if(account)
			{
				arr_Account[input_i]=8;
				Character_account='8';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=8;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}
		else if(x>630 && x<762)
		{
			if(account)
			{
				arr_Account[input_i]=9;
				Character_account='9';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=9;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}
		/***********************************退格，0，小数点（都是后加的）*****************************************/
		/*else if(x > 630 && x < 762)
		{
			// 退格功能
			if(account && input_i > 0)
			{
				input_i--;
				account_x -= 20;
				arr_Account[input_i] = -1; // 可选：清空该位(读取数组时，可自动跳过-1)
				// 用背景色覆盖字符
				char blank = ' ';
				Display_characterX(account_x, account_y, &blank, 0xFFFFFF, 2); 
			}
			if(password && input_j > 0)
			{
				input_j--;
				password_x -= 20;
				arr_Password[input_j] = -1; // 可选
				char blank = ' ';
				Display_characterX(password_x, password_y, &blank, 0xFFFFFF, 3); 
			}
		}
		else if(x>630 && x<762)
		{
			if(account)
			{
				arr_Account[input_i]=0;
				Character_account='0';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=0;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}*/
		
		/*最好用字符数组实现
		else if(x>630 && x<762)
		{
			if(account)
			{
				arr_Account[input_i]='.';
				Character_account='.';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]='.';
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}*/

	}
	else if(y>360 && y<422)
	{
		/*if(x>389 && x<503)
		{
			return_welcome=1;
		}*/
		if(x>503 && x<630)
		{
			if(account)
			{
				arr_Account[input_i]=0;
				Character_account='0';
				Display_characterX(account_x,account_y,&Character_account,0x000000,2); 
				account_x+=20;
				input_i++;
			}
			if(password)
			{
				arr_Password[input_j]=0;
				Display_characterX(password_x,password_y,&Character_password,0x000000,3); 
				password_x+=20;
				input_j++;
			}
		}
	}
}
/****************************************************************/

//时间线程
pthread_t time_tid;
int stop_time_tid=0;
//显示id线程
pthread_t id_tid;
int stop_id_tid=0;


void main_page(void)
{
    Clean_Area(14,345,631,135,0XFFFFFF);
    stop_time_tid=0;
	stop_id_tid=0;
    show_bmp(0,0,"main.bmp");

	// 启动线程（只创建一次）
	pthread_create(&time_tid, NULL, show_time, NULL);
	pthread_create(&id_tid, NULL, show_id, NULL);
	/*********/
	while(1)
	{
		gets_pos(&x,&y);
		if(x>0&&x<175&&y>425&&y<475)//退出登录
		{
			stop_id_tid=1;
			stop_time_tid=1;

			memset(arr_Account,0,sizeof(arr_Account));//清除账号
			memset(arr_Password,0,sizeof(arr_Password));//清除密码
			account=1;
			password=0;
			account_x=120;
			account_y=245;

			// 等待线程退出
            pthread_join(time_tid, NULL);
            pthread_join(id_tid, NULL);

			current_page = LOGIN_PAGE; // 切换回登录页
            return;

		}
		/*********/	
		else if(y>150 && y<325)
		{
			if(x>25 && x<175)//充电卡充电
			{
				current_page = CARD_PAGE; // 切换充电卡充电页
            	return;
			}
			else if(x>225 && x<375)//扫码充电
			{
				current_page = SCANTOPAY_PAGE; // 切换扫码充电页
            	return;
			}
			else if(x>425 && x<575)//账户信息
			{
				current_page =INFO_PAGE; // 切换信息页
            	return;
			}

		}
		/*********/	
		else
		{
			printf("wating\n");
			//gets_pos(&x,&y);
			//printf("(%d,%d)\n",x,y);
		}

	}
    
}
/**************************************************************/
void show_font(int *lcd_mp, char *buf,int x ,int y);//声明

void card_page()
{
	//结束这两个线程
	stop_id_tid=1;
	stop_time_tid=1;
	// 等待线程退出
	pthread_join(time_tid, NULL);
	pthread_join(id_tid, NULL);
	show_bmp(0,0,"card.bmp");
	gets_pos(&x,&y);
	if(x>0 && x<100 &&y>0 &&y<100)
	{
		current_page=MAIN_PAGE;
		return;
	}
}

char amount[8];//记录输入的值（包括'\0'）,最多7位（包含小数点）
int cnt=0;//计数
int x_start=200;//起始坐标

float result;//字符拼接为浮点型的结果
float change(char *amount)
{
    int i = 0;
    int point_flag = 0;
    float decimal_place = 0.1;

    while (i < 7)
    {
		if(amount[i]!='-')
		{
			if (amount[i] == '.')
			{
				point_flag = 1;
			}
			else if (!point_flag)
			{
				result = result * 10 + (amount[i] - '0');
			}
			else
			{
				result = result + (amount[i] - '0') * decimal_place;
				decimal_place /= 10;
			}
			i++;
		}   
    }
    return result;
}

char ch;
int money_x=175;
void FLoat_lock(void)
{
    if (cnt >= 7) return; // 最多输入7位（含.）
    gets_pos(&x, &y);

    if (y > 100 && y < 150) // 1 2 3
    {
        if (x > 450 && x < 525)
		{
			amount[cnt]='1';
			ch = '1';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
        if (x > 550 && x < 625)
		{
			amount[cnt]='2';
			ch = '2';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
        if (x > 650 && x < 725)
		{
			amount[cnt]='3';
			ch = '3';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
    }
    else if (y > 175 && y < 225) // 4 5 6
    {
		if (x > 450 && x < 525)
		{
			amount[cnt]='4';
			ch = '4';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
        if (x > 550 && x < 625)
		{
			amount[cnt]='5';
			ch = '5';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
        if (x > 650 && x < 725)
		{
			amount[cnt]='6';
			ch = '6';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
    }
    else if (y > 250 && y < 300) // 7 8 9
    {
		if (x > 450 && x < 525)
		{
			amount[cnt]='7';
			ch = '1';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
        if (x > 550 && x < 625)
		{
			amount[cnt]='8';
			ch = '2';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
        if (x > 650 && x < 725)
		{
			amount[cnt]='9';
			ch = '3';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
		}
    }
    else if (y > 325 && y < 375) // . 0 删除
    {
        if (x > 450 && x < 525)
        {
            // 小数点最多输入1个
            if (strchr(amount, '.') == NULL)
			{
				amount[cnt]='.';
				ch = '.';
				Display_characterX(money_x,250,&ch,0x000000,2); 
				money_x+=20;
				cnt++;
			}
            else return;
        }
        else if (x > 550 && x < 625)
        {
            amount[cnt]='0';
			ch = '0';
			Display_characterX(money_x,250,&ch,0x000000,2); 
			money_x+=20;
			cnt++;
        }
        else if (x > 650 && x < 725)
        {
            if (cnt > 0)
            {
                cnt--;
                money_x -= 20;
                amount[cnt] = '\0';
                char blank = ' ';
                Display_characterX(money_x, 250, &blank, 0xFFFFFF, 2);
			}
        }
    }
}
 
void scantopay_page()
{
	Clean_Area(175,250,150,100,0XFFFFFF);
	show_bmp(0,0,"saoma1.bmp");
	stop_time_tid=0;
	stop_id_tid=0;

	// 启动线程（只创建一次）
	pthread_create(&time_tid, NULL, show_time, NULL);
	pthread_create(&id_tid, NULL, show_id, NULL);

	memset(amount, 0, sizeof(amount));//复制0到amount所指的字符串前sizeof(amount)字节中
    cnt = 0;
    x_start = 200;
    result = 0.0;

	while(1)
	{
		//if(!stop_time_tid) pthread_create(&time_tid,NULL,show_time,NULL);
		//if(!stop_id_tid) pthread_create(&id_tid,NULL,show_id,NULL);
		gets_pos(&x,&y);
		if(y>175&&y<225)
		{
			if(x>50&&x<150)//30
			{
				result=30;//金额
				sprintf(amount, "%.2f", result);
                show_font(p, amount, 25, 350);
			}
			else if(x>175&&x<275)//50
			{
				result=50;//金额
				sprintf(amount, "%.2f", result);
                show_font(p, amount, 25, 350);
			}
		}
	
		FLoat_lock();

		gets_pos(&x,&y);
		if(y>400 && y<450)
		{
			if(x>450 && x<575)//返回
			{
				current_page=MAIN_PAGE;
				return;
			}
			else if(x>600 && x<725)//确认
			{
				if (cnt > 0)
					result = change(amount); // 输入转浮点
				//展示二维码
				show_bmp(0, 0, "QR code.bmp");
				//倒计时
				char countdown[10];
				for (int i = 10; i >= 0; i--)
				{
					sprintf(countdown, "%03d", i);
					Clean_Area(300, 25, 380, 65, 0xFFFFFF); // 清屏区域
					show_font(p, countdown, 300, 100);
					sleep(1);
				}
				current_page = CHARGINGMODE_PAGE;
				return;
			}	
		}
	}
}
/************************************************************************ */
void mode_page()
{
	//结束这两个线程
	stop_id_tid=1;
	stop_time_tid=1;
	// 等待线程退出
	pthread_join(time_tid, NULL);
	pthread_join(id_tid, NULL);

	Clean_Area(14,345,631,135,0XFFFFFF);
	show_bmp(0,0,"mode.bmp");
	while(1)
	{
		gets_pos(&x,&y);
		if(y>200&&y<350)
		{
			if(x>100 && x<250)
			{
				//快充，开始充电
				show_bmp(0,0,"fast.bmp");
				gets_pos(&x,&y);
				if(x>325&&x<475&&y>150&&y<325)
				{
					current_page=CHARGINGINFO1_PAGE;
					return;
				}	
			}
			else if(x>550 && x<700)
			{
				//慢充
				show_bmp(0,0,"slow.bmp");
				gets_pos(&x,&y);
				if(x>325&&x<475&&y>150&&y<325)
				{
					current_page=CHARGINGINFO1_PAGE;
					return;
				}
			}
		}
		else if(x>0 && x<100 && y>0 && y<100)
		{
			current_page=SCANTOPAY_PAGE;
			return;
		}
	}
}

/**************************************************************************** */
void show_jpg(char * filename,int start_x,int start_y)
{
    /* This struct contains the JPEG decompression parameters and pointers to
    * working space (which is allocated as needed by the JPEG library).
    */
    struct jpeg_decompress_struct cinfo;
    /* We use our private extension JPEG error handler.
    * Note that this struct must live as long as the main JPEG parameter
    * struct, to avoid dangling-pointer problems.
    */
    struct jpeg_error_mgr jerr;
    /* More stuff */
    FILE * infile;			/* source file */
    unsigned char * buffer;		/* Output row buffer */
    int row_stride;			/* physical row width in output buffer */

    /* In this example we want to open the input file before doing anything else,
    * so that the setjmp() error recovery below can assume the file is open.
    * VERY IMPORTANT: use "b" option to fopen() if you are on a machine that
    * requires it in order to read binary files.
    */

    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return ;
    }

    /* Step 1: allocate and initialize JPEG decompression object */

    /* We set up the normal JPEG error routines, then override error_exit. */
    cinfo.err = jpeg_std_error(&jerr);

    /* Now we can initialize the JPEG decompression object. */
    jpeg_create_decompress(&cinfo);

    /* Step 2: specify data source (eg, a file) */

    jpeg_stdio_src(&cinfo, infile);

    /* Step 3: read file parameters with jpeg_read_header() */

    (void) jpeg_read_header(&cinfo, TRUE);
    /* We can ignore the return value from jpeg_read_header since
    *   (a) suspension is not possible with the stdio data source, and
    *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
    * See libjpeg.txt for more info.
    */

    /* Step 4: set parameters for decompression */

    /* In this example, we don't need to change any of the defaults set by
    * jpeg_read_header(), so we do nothing here.
    */

    /* Step 5: Start decompressor */

    (void) jpeg_start_decompress(&cinfo);
    /* We can ignore the return value since suspension is not possible
    * with the stdio data source.
    */

    /* We may need to do some setup of our own at this point before reading
    * the data.  After jpeg_start_decompress() we have the correct scaled
    * output image dimensions available, as well as the output colormap
    * if we asked for color quantization.
    * In this example, we need to make an output work buffer of the right size.
    */ 
    /* JSAMPLEs per row in output buffer */
    row_stride = cinfo.output_width * cinfo.output_components;  //计算一行的大小

    /* Make a one-row-high sample array that will go away when done with image */
    buffer = calloc(1,row_stride);

    /* Step 6: while (scan lines remain to be read) */
    /*           jpeg_read_scanlines(...); */

    /* Here we use the library's state variable cinfo.output_scanline as the
    * loop counter, so that we don't have to keep track ourselves.
    */
    int data = 0;

    while (cinfo.output_scanline < cinfo.output_height) 
    {
        /* jpeg_read_scanlines expects an array of pointers to scanlines.
        * Here the array is only one element long, but you could ask for
        * more than one scanline at a time if that's more convenient.
        */
        (void) jpeg_read_scanlines(&cinfo, &buffer, 1); //从上到下，从左到右  RGB RGB RGB RGB 
        
        for (int i = 0; i < cinfo.output_width; ++i)  //012 345
        {
            data |= buffer[3*i]<<16;	//R
            data |= buffer[3*i+1]<<8;	//G
            data |= buffer[3*i+2];  	//B 

            //把像素点写入到LCD的指定位置
            lcd_mp[800*start_y + start_x + 800*(cinfo.output_scanline-1) + i] = data;

            data = 0;
        }
    
    }

    /* Step 7: Finish decompression */

    (void) jpeg_finish_decompress(&cinfo);
    /* We can ignore the return value since suspension is not possible
    * with the stdio data source.
    */

    /* Step 8: Release JPEG decompression object */

    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_decompress(&cinfo);

    /* After finish_decompress, we can close the input file.
    * Here we postpone it until after no more JPEG errors are possible,
    * so as to simplify the setjmp error logic above.  (Actually, I don't
    * think that jpeg_destroy can do an error exit, but why assume anything...)
    */
    fclose(infile);

    /* At this point you may want to check to see whether any corrupt-data
    * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
    */

    /* And we're done! */
    return ;
}

/******************************************************************************** */
//充电时间线程
pthread_t Ctime_tid;
int stop_Ctime_tid=0;
//显示充电动画线程
pthread_t Cgif_tid;
int stop_Cgif_tid=0;

//int n = 5;	printf("%02d\n", n);	输出为:05
int s = 0, m = 0, h = 0;
void *show_Ctime(void *arg) // 计时器,pthread 线程函数需返回 void *
{
    while (!stop_Ctime_tid)
    {
        s++;
        if (s >= 60)
        {
            m++;
            s = 0;
        }
        if (m >= 60)
        {
            h++;
            m = 0;
        }
        // 构造时间字符串，例如 "01:02:03"
        char Ctime_buf[16];
        sprintf(Ctime_buf, "%02d:%02d:%02d", h, m, s);

        // 清除旧时间显示区域，防止重影
        Clean_Area(650, 75, 650 + 120, 75 + 40, 0xFFFFFF);
        // 显示新时间
        show_font(p, Ctime_buf, 650, 75);
        sleep(1); // 每秒更新一次
    }
    pthread_exit(NULL);
}

void *show_Cgif(void *arg)
{
	while (!stop_Cgif_tid)
	{
		for (int i = 0; i <= 86 && !stop_Cgif_tid; ++i)
		{
			char gif_buf[64];
			sprintf(gif_buf, "./gif/Frame%d.jpg", i);
			Clean_Area(625, 225, 625 + 150, 225 + 150, 0xFFFFFF); // 根据图片大小调整
			show_jpg(gif_buf, 625, 225);
			usleep(20000); // 控制帧率
		}
	}
	pthread_exit(NULL);
}

//电量显示线程
pthread_t electric_tid;
int stop_electric_tid=0;
//费用显示线程
pthread_t cost_tid;
int stop_cost_tid=0;

float electric=0;
void *show_electric(void *arg)//每秒涨0.03
{
	while(!stop_electric_tid)
	{
		electric+=0.03;
		
        // 清除旧显示区域，防止重影
        Clean_Area(200, 150, 200 + 120, 150+ 40, 0xFFFFFF);

		char electric_buf[32];
        sprintf(electric_buf, "%.2f",electric);
        show_font(p, electric_buf, 200, 150);
        sleep(1); // 每秒更新一次
    }
    pthread_exit(NULL);
}

float cost=0;
void *show_cost(void *arg)//每秒涨0.032
{
	while(!stop_cost_tid)
	{
		cost+=0.032;
		
        // 清除旧显示区域，防止重影
        Clean_Area(425, 150, 425 + 120, 150+ 40, 0xFFFFFF);

		char cost_buf[32];
        sprintf(cost_buf, "%.2f",cost);
        show_font(p, cost_buf, 425, 150);
        sleep(1); // 每秒更新一次
    }
    pthread_exit(NULL);
}

// 电流变化线程
pthread_t A_tid;
int stop_A_tid = 0;

// 电压变化线程
pthread_t V_tid;
int stop_V_tid = 0;

// 在指定范围生成一个带1位小数的随机浮点数
float rand_float_range(float min, float max)
{
    return ((float)rand() / RAND_MAX) * (max - min) + min;
}

void *show_A(void *arg) // 电流范围：156.7A ~ 160.2A
{
    srand(time(NULL)); // 保证每次运行随机性不同
    while (!stop_A_tid)
    {
        float current = rand_float_range(156.7f, 160.2f);

        char A_buf[16];
        sprintf(A_buf, "%.1fA", current); // 保留1位小数

        Clean_Area(100, 200, 220, 240, 0xFFFFFF); // 清空显示区域
        show_font(p, A_buf, 100, 200);            // 显示电流值
        sleep(1); // 每秒更新一次
    }
    pthread_exit(NULL);
}

void *show_V(void *arg) // 电压范围：378.6V ~ 382.6V
{
    srand(time(NULL) + 1); // 避免与电流线程使用相同种子
    while (!stop_V_tid)
    {
        float voltage = rand_float_range(378.6f, 382.6f);

        char V_buf[16];
        sprintf(V_buf, "%.1fV", voltage); // 保留1位小数

        Clean_Area(100, 250, 220, 290, 0xFFFFFF); // 清空显示区域
        show_font(p, V_buf, 100, 250);            // 显示电压值
        sleep(1); // 每秒更新一次
    }
    pthread_exit(NULL);
}

/***************************************************************************** */
//特斯拉Model S,电费默认1.2元一度，电池容量55KWH，充电损耗 交流：12%，直流：7%
//直流电费用:从x%~100%->1.2*(((100-x)/100)*55/(1-0.07)) 20%~100%大约57.6元
void info1_page()
{
	//清屏
	Clean_Area(175,250,150,100,0XFFFFFF);
	show_bmp(0,0,"info1.bmp");

	// 只创建一次线程
	static int thread_created = 0;
	if(thread_created == 0)
	{
		stop_time_tid=0;
		stop_id_tid=0;
		stop_Ctime_tid=0;
		stop_Cgif_tid=0;
		stop_cost_tid=0;
		stop_electric_tid=0;
		stop_A_tid=0;
		stop_V_tid=0;

		pthread_create(&time_tid, NULL, show_time, NULL);
		pthread_create(&id_tid, NULL, show_id, NULL);
		pthread_create(&Ctime_tid,NULL, show_Ctime, NULL);
		pthread_create(&electric_tid, NULL, show_electric, NULL);  // 持续更新
		pthread_create(&cost_tid, NULL, show_cost, NULL);
		pthread_create(&A_tid, NULL, show_A, NULL);
		pthread_create(&V_tid, NULL, show_V, NULL);

		thread_created = 1;
	}
	while(1)
	{
		//30min充满,电量每秒涨0.03，费用每秒涨0.032
		gets_pos(&x,&y);
		if(x>25 && x<100 && y>275 && y<350)
		{
			current_page=CHARGINGINFO2_PAGE;
			return;
		}
		else if(x>625 && x<775 && y>350 && y<450)//结束充电
		{
			current_page=EXIT_PAGE;
			return;
		}
	}
}

void info2_page()
{
	// 用大图覆盖电量等区域，不影响后台线程数据继续增加
	show_bmp(150,50,"info2.bmp");
	while(1)
	{
		gets_pos(&x,&y);
		if(x>25 && x<100 && y>75 && y<150)
		{
			current_page=CHARGINGINFO1_PAGE;
			return;
		}
		else if(x>625 && x<775 && y>350 && y<450)//结束充电
		{
			current_page=EXIT_PAGE;
			return;
		}
	}
}

/******************************************************************* */

void exit_page()
{
	stop_time_tid = 1;
	stop_id_tid = 1;
	stop_Ctime_tid = 1;
	stop_Cgif_tid = 1;
	stop_cost_tid = 1;
	stop_electric_tid = 1;
	stop_A_tid = 1;
	stop_V_tid = 1;

	pthread_join(time_tid, NULL);
	pthread_join(id_tid, NULL);
	pthread_join(Ctime_tid, NULL);
	pthread_join(electric_tid, NULL);
	pthread_join(cost_tid, NULL);
	pthread_join(A_tid, NULL);
	pthread_join(V_tid, NULL);

	show_bmp(0,0,"last.bmp");
	printf("欢迎下次使用");
}

/******************************线程***********************************/
void show_font(int *lcd_mp, char *buf,int x ,int y)
{
    // 打开字体
    font *f = fontLoad("/usr/share/fonts/DroidSansFallback.ttf");

    // 字体大小的设置
    fontSetSize(f, 30);

    // 创建一个画板（点阵图）宽是200个像素，高是50个像素
    bitmap *bm = createBitmapWithInit(220, 50, 4, getColor(0, 0, 0, 255)); // 也可使用createBitmapWithInit函数，改变画板颜色

    // 将字体写到点阵图上 0,0表示汉字在画板的左上角显示
    fontPrint(f, bm, 10, 10, buf, getColor(0, 255, 255, 255), 0);

    // 把字体框输出到LCD屏幕上  参数0,0表示画板显示的坐标位置
    show_font_to_lcd(lcd_mp, x, y, bm);

    // 关闭字体，关闭画板
    fontUnload(f);
    destroyBitmap(bm);
}
// 获取时间
char *get_time()
{
    time_t rawtime;
    struct tm *timeinfo;
    char *buffer = (char *)malloc(sizeof(char) * 80);
	time(&rawtime);
	timeinfo = localtime(&rawtime);

	// 格式化时间到 buffer 字符串
	snprintf(buffer, 80, "%04d-%02d-%02d %02d:%02d:%02d",
			timeinfo->tm_year + 1900, // 年份是从1900年开始计算的，所以要加1900
			timeinfo->tm_mon + 1,     // 月份是从0开始的，所以要加1
			timeinfo->tm_mday,        // 日期
			timeinfo->tm_hour,        // 小时
			timeinfo->tm_min,         // 分钟
			timeinfo->tm_sec);        // 秒
	return buffer;
}
    
void *show_time(void *arg)
{
	while(!stop_time_tid)
	{
		char *buf_time = get_time();
        show_font(p, buf_time,300,0);
		sleep(1);                // 每秒更新一次
	}
	pthread_exit(NULL);
}

// 获取ID并显示在左上角，格式为：ID:1234
void *show_id(void *arg)
{
    while (!stop_id_tid)
    {
        // 清除显示区域（根据字符长度清理足够宽）
        Clean_Area(10, 10, 200, 40, 0xFFFFFF); 
        // 显示 "ID:"
        Display_characterX(10, 10, (unsigned char *)"ID:", 0x000000, 2);
        int x = 10 + 3 * 20; // 从 "ID:" 后面开始显示数字，假设字号为2倍，字符宽约20px
        for (int i = 0; i < 4; i++)
        {
            if (arr_Account[i] < 0 || arr_Account[i] > 9) break; // 保守判断

            char ch = arr_Account[i] + '0'; // 转换为字符
            char str[2] = {ch, '\0'};

            Display_characterX(x, 10, (unsigned char *)str, 0x000000, 2);
            x += 10 * 2;
        }
        usleep(500000); // 每 0.5 秒刷新一次
    }
    pthread_exit(NULL);
}


void run_ui()
{
    while (current_page != EXIT_PAGE)
    {
        switch (current_page)
        {
            case LOGIN_PAGE: login(); break;
			case REGIST_PAGE:regist();break;
            case MAIN_PAGE:  main_page(); break;
			case CARD_PAGE: card_page(); break;
            case SCANTOPAY_PAGE:scantopay_page();break;
			case CHARGINGMODE_PAGE:mode_page();break;
			case CHARGINGINFO1_PAGE:info1_page();break;
			case CHARGINGINFO2_PAGE:info2_page();break;
			case EXIT_PAGE:exit_page();break;
            default: break;
        }
    }
}


int main(int argc,const char*argv[])
{
    init();
    Init_Font();
	for (int i = 0; i <= 126; ++i)
	{
		char gif_buf[64];
		sprintf(gif_buf, "./ovo/Frame%d.jpg", i);
		show_jpg(gif_buf, 0, 0);
		usleep(20000); // 控制帧率 50Hz
	}
	/*while(1)
    {
        show_bmp(0,0,"welcome.bmp");
        gets_pos(&x,&y);
        if(x>login_min_X&&x<login_max_X && y>login_min_y&&y<login_max_y)
		{
			login();
		}
		
    }*/
   	current_page=LOGIN_PAGE;
	while(1)
	{
		run_ui();
	}
    uninit();
	return 0;
}