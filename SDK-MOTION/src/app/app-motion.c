#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/wait.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

/*
1. 检测到图像变化，报警
2. 检测到外部中断，报警
3. 3分钟能连续检测图像变化20次，则暂停2小时，2小时后在开启
*/
/*define globe variable*/

/*play_pid:当前播放的MP3子进程ID*/
unsigned int play_pid = 0;

/*gradchild:当前播放的MP3孙子进程ID*/
unsigned int gradchild = 0;

unsigned int play_flag;

/*共享内存描述标记
sharemem:
 byte1:孙子进程ID号
 byte2:是否有MP3播放标识play_flag_2
 //byte3:MP3播放次数
*/
int shmid;
char *p_addr;

#define PERM S_IRUSR|S_IWUSR 



/*报警铃声歌曲名,song1图像变化报警铃声
song2外部中断报警铃声*/
char *song1="11.mp3";
char *song2="22.mp3";
//char *song="234.mp3";


/*定时器时间为3分钟*/
#define THREE_ALARM 3*60
/*睡眠时间为2小时*/
#define SLEEP_TIME 2*60*60
#define  CPM_CNT 20
int threemin_alarm = 1;

static int con_cnt=0;
int sleep_flag = 0;
unsigned int time_tmp;

#define max(flag) (flag) >1 ? "pic":"key"
//#define DEBUG
int alarm_flag;

static int pic_cnt;
int cnt_fd;
/*计算图像变化次数，超过2次则认为有图像变化*/
#define COMPARE_CNT 5


/*************************************************
Function name: count_pic
Called by		 : 函数main
Parameter    : void
Description	 : 计算图片变化数
Return		   : int
Autor & date : ykz 10.4.25
**************************************************/
int count_pic(void)
{
	int fd,ret;
	char *buf;
	buf = (char *)malloc(10);

	system("ls /root/motion | wc -l > count.txt");
	lseek(cnt_fd, 0 ,SEEK_SET);
	ret = read(cnt_fd , buf , 10);
	if(ret)
	  ret = atoi(buf);
	
	free (buf);
	return ret;
}


/*************************************************
Function name: my_func_sleepalarm
Called by		 : 函数my_func_3alarm
Parameter    : sing_no
Description	 : 3分钟连续变化20次后，延时2小时后的唤醒函数
Return		   : void
Autor & date : ykz 10.4.25
**************************************************/
void my_func_sleepalarm(int sign_no)
{

	if( sign_no == SIGALRM){
		printf("Now sleep time finished!start a new work!\n");
		/*则睡眠表示sleep_flag为0*/
		sleep_flag = 0;
	}//end sing_no
}


/*************************************************
Function name: my_func_3alarm
Called by		 : 函数restart_caculate_play
Parameter    : sing_no
Description	 : 每隔3分钟检测图像连续变化是否超过20次
Return		   : void
Autor & date : ykz 10.4.25
**************************************************/
void my_func_3alarm(int sign_no)
{

	if( sign_no == SIGALRM){

		struct tm *p;
		time_t timep;
		unsigned int second;
	  printf("\n------------------warning------------------\n");
	  printf("[%s]3alarm!con_cnt=%d\n",max(alarm_flag),con_cnt);
	  /*判断图像运动次数是否超过20次
	   如果是：则系统睡眠2小时；
	   不是，则系统继续添加下一个3分钟的定时器*/
	  if( con_cnt >= CPM_CNT){
		  if(play_flag){
		    kill(play_pid,SIGKILL);
			kill(gradchild,SIGKILL);
			wait(NULL);
		  }
		  //sleep (1);
		time(&timep);
		p = localtime(&timep);
		second = (p->tm_hour)*60*60 + (p->tm_min) * 60 + p->tm_sec;
		time_tmp=second;

		  printf("Now maybe it is rainning!,system will sleep %d seconds!\n",SLEEP_TIME);
		  con_cnt = 0;
		  /*置睡眠标识sleep_flag为1*/
		  sleep_flag = 1;
		  /*系统睡眠2小时*/
		  if(signal(SIGALRM,my_func_sleepalarm)<0)
		    perror("signal");
		  alarm(SLEEP_TIME);
	 }
	 else{
	   printf("Nornal test!not rainning!\n");
	   con_cnt = 0;
	   /*添加下一个3分钟的定时器*/
	   if(signal(SIGALRM,my_func_3alarm)<0)
		  perror("signal");
	   alarm(THREE_ALARM);		  
	 }

	  printf("\n------------------warning------------------\n");
	} //end sing_no
}



/*************************************************
Function name: play
Called by		 : 函数startplaymp3
Parameter    : void
Description	 : 子进程创建孙子进程播放MP3
Return		   : void
Autor & date : ykz 10.4.2
**************************************************/
void play(void)
{

	pid_t fd;
	char *c_addr;
	char *song_name;
	int play_flag_gradchild1=1;
	int play_flag_gradchild2=2;
	int i=0;

		/*创建孙子进程*/
		fd = fork();
		if(fd == -1)
		{	
			perror("fork");
			exit(1);
		}
		else if(fd == 0) /*孙子进程，播放MP3*/
		{

			printf("\n--------------play mp3----------------\n");
			if(alarm_flag==1)
			  song_name=song1;
			else song_name=song1;
			printf("THIS SONG IS %s\n",song_name);
			/*使用madplay播放MP3*/
			execl("/motion/madplay","madplay",song_name,NULL);
			printf("\n\n\n");
		}
		else  /*子进程*/
		{
		
			/*把孙子进程的id传入共享内存*/
			memcpy(c_addr,&fd,sizeof(pid_t));
			
			/*目前在播放MP3，将播放标记传入共享内存*/
			memcpy(c_addr+sizeof(int),&play_flag_gradchild1,4);
			
			/*等待孙子进程结束，只要结束：
			传回play_flag_gradchild2=2,表示现在MP3没有播放*/
			if(fd == wait(NULL))
			{
				printf("\n------------------warning------------------\n");
				
			/*通过共享内存传回play_flag_gradchild2=2,表明后面的一定不是连续的MP3播放*/
					memcpy(c_addr+sizeof(int),&play_flag_gradchild2,4);
					printf("Gradchild normal finish!\n");
		      printf("------------------warning------------------\n");
			}//end if(fd == wait(NULL))
		}

}



/*************************************************
Function name: startplaymp3
Called by		 : 函数caculate_play,restart_caculate_play
Parameter    : pid_t *childpid
Description	 : 主进程创建子进程
Return		   : void
Autor & date : ykz 10.4.2
**************************************************/
void startplaymp3(pid_t *childpid,int flag)
{

		int ret = 0;
	  pid_t cun_pid;
		/*创建子进程*/
		cun_pid = fork();
		
		if(cun_pid == -1)
		{	
			perror("son fork");
			exit(1);
		}

		if(cun_pid == 0) /*子进程*/
		  play();
		

		if(cun_pid > 0) /*父进程*/
		{
			*childpid = cun_pid;
		  sleep(1);  /*让孙子进程先执行*/
		  
		  /*如果是图像运动变化，将全局变量con_cnt加1*/
		  if( flag == 2 )
			con_cnt++;
		  
		  printf("\nNow con_cnt=%d\n",con_cnt);
		  /*把孙子进程的pid传给父进程*/
		  memcpy(&gradchild,p_addr,sizeof(pid_t));

		}

}

/*************************************************
Function name: caculate_play
Called by		 : 函数key_pic_mp3
Parameter    : int flag
Description	 : 连续播放时，计算连续播放时间，调用函数startplaymp3开始新的播放
Return		   : int
Autor & date : ykz 10.4.2
**************************************************/
int caculate_play(int flag)
{

	int ret;
	   
	/*kill掉当前播放MP3的子进程，孙子进程*/
		
		kill(play_pid,SIGKILL);
		kill(gradchild,SIGKILL);
		wait(NULL);
	  
	/*将共享内存清空*/
		memset(p_addr,'\0',1024);

	 startplaymp3(&play_pid , flag);
   return 1;
}


/*************************************************
Function name: restart_caculate_play
Called by		 : 函数key_pic_mp3
Parameter    : int flag
Description	 : 未超过连续播放时间时，有新的图像或者外部中断检测到时调用函数
Return		   : void
Autor & date : ykz 10.4.2
**************************************************/
void restart_caculate_play(int flag)
{

		int ret;
		/*add 3 minute alarm*/
		if(threemin_alarm){
			if(signal(SIGALRM,my_func_3alarm)<0)
				perror("signal");
			ret = alarm(THREE_ALARM);
			printf("[%s]Start add 3 minute alarm!\n",max(flag));
			threemin_alarm = 0;
		}


#if 1 
    /*判断是否有子进程，或者孙子进程，如果有则KILL掉*/
		if(play_flag){
			kill(play_pid,SIGKILL);
			kill(gradchild,SIGKILL);
			wait(NULL);
			//sleep(1);
		}
#endif
		play_pid = 0;
		gradchild = 0;
		
		memset(p_addr,'\0',1024);
	
	  /*开始播放MP3*/
	  startplaymp3(&play_pid,flag);
}



/*************************************************
Function name: key_pic_mp3
Called by		 : 函数main
Parameter    : int flag
Description	 : 当检测到有外部中断，或者图像变化时处理函数
Return		   : int
Autor & date : ykz 10.4.2
**************************************************/
int key_pic_mp3(int flag)
{

  printf("------------------------- KEY_PIC_MP3 ----------------------------\n");
	printf("[%s]Now detect ** %s ** have change\n",max(flag),max(flag));
	int ret = 0;
	int over_flag_2; 
	int play_flag_2;

  /*sleep_flag 用于判断，检测系统是否处于2小时的睡眠状态*/
	if(sleep_flag){
		printf("It is rainning!system is sleeping!\n");
		struct tm *p; 
		time_t timep;
		unsigned int second;
		
		time(&timep);
		p = localtime(&timep);
		second = (p->tm_hour)*60*60 + (p->tm_min) * 60 + p->tm_sec;

		printf("summary time:%d seconds,sleep:%d seconds,need sleep:%d seconds\n",
					SLEEP_TIME,second-time_tmp,SLEEP_TIME-(second-time_tmp));

		return 0;
	}
	
	alarm_flag = flag;


	/*从sharemem中读出是否有MP3处在播放状态标识*/
	memcpy(&play_flag_2,p_addr + sizeof(int),sizeof(int));
//	printf("===>key_pic_mp3,play_flag_2=%d\n",play_flag_2);
	
	play_flag = play_flag_2;
	/*play_flag_2:当前是否有MP3在播放
	0：当前没有MP3播放
  1:子进程当前处在MP3播放状态
  2:孙子当前播放MP3正常结束，且当前没有MP3播放*/

	/*当前有MP3在播放*/
	if(play_flag_2 == 1){
			printf("[%s]Yes,have mp3 play\n",max(flag));
			/*调用*/
			ret = caculate_play(flag);
	}
	
	/*当前无MP3在播放*/
	else{
			printf("[%s]No, have not mp3 play\n",max(flag));		
		   restart_caculate_play(flag);
	}
	
	
		return ret;
}



/*************************************************
Function name: main
Called by		 : 
Parameter    : void
Description	 : 主函数，检测按键是否有按下，通过pic.txt检测图像是否有变化
Return		   : int
Autor & date : ykz 10.4.2
**************************************************/
main(void)
{
	
	 int buttons_fd,pic_fd;
	 char pic_buf[1];
	 int key_value;
	 int flag;
	 int ret;
	 int tmp_cnt;
	 /*设备文件的打开*/
	 buttons_fd = open("/dev/buttons", 0);
	 if(buttons_fd < 0) {
		perror("open device buttons");
		exit(1);
	 }
	 printf("open buttons sucess!\n"); 
	 
	 
	 
	 /*文件pic.txt记录是否有图像变化。
	   1：有图像变化
	   0：没有图像变化*/  
	 pic_fd = open("pic.txt",O_RDWR | O_CREAT,0666);
	 if(pic_fd < 0) {
		perror("open pic.txt");
		exit(1);
	 }
	 printf("open pic.txt success!\n");
	
	 /*文件count.txt用于图像连续变化时，记录/root/motion中图片张数*/
	 cnt_fd = open("count.txt",O_RDWR | O_CREAT,0666);
	 if(cnt_fd < 0){
		perror("open count.txt");
		exit(1);
	 }
	 printf("open count.txt success!\n");
	 system("ls > count.txt");
	 
	 /*共享内存申请*/
	 	if((shmid = shmget(IPC_PRIVATE,20,PERM))== -1)
			exit(1);
		p_addr = shmat(shmid,0,0);
		memset(p_addr,'\0',1024);
		
		
	/*主循环，首先判断是外部中断还是图像变化*/
	while(1){
	
		/*外部中断检测,监听获取键值*/
			ret = read(buttons_fd, &key_value, sizeof key_value);
			if (ret != sizeof key_value) 		
					perror("read buttons\n");	 
			else {
				if(key_value){
					printf("====================================================================\n");
					printf("\n\n\n====================================================================\n");
					printf("[type1]button detect,buttons_value: %d\n", key_value);
					/*外部中断处理*/
					key_pic_mp3(1);
				}
			} //end else
		
		
		
		/*图形变化检测,当有图像变化时motion会产生一个事件，
		事件处理为脚本/motion/appon，该脚本先点亮LED灯；
		然后向文件/motion/pic.txt写入字符"1"表明现在有图像变化被检测到；
		
		读取文件/motion/pic.txt第一个字符
		 0：没有图像变化
		 1：有图像变化*/
		 
		 lseek(pic_fd, 0 ,SEEK_SET);
		 ret = read(pic_fd, pic_buf, 1);

  	 if(ret==1)
		 {
			if(pic_buf[0] == '1'){ /*有图像变化被检测到*/
			 printf("====================================================================\n");
			 printf("\n\n\n====================================================================\n");
			 printf("[type2]pic motion detect!\n");
			 lseek(pic_fd, 0 ,SEEK_SET);
			
			 if((ret = write(pic_fd, "0", 1)))
				  lseek(pic_fd, 0 ,SEEK_SET);
			 
//			 system("rm /root/motion/* -rf");
			 pic_cnt = count_pic();
			 //printf("first pic_cnt=%d\n",pic_cnt);
			 /*图像运动变化处理*/
		 	 key_pic_mp3(2);
		 }
		
			else if(pic_buf[0] == '0'){
			 tmp_cnt = count_pic();
			 if( (tmp_cnt-pic_cnt) > COMPARE_CNT){
				printf("====================================================================\n");
				printf("\n\n\n====================================================================\n");
				printf("[type3]pic motion detect!\n");
			
//				system("rm /root/motion/* -rf");
				pic_cnt = count_pic();
				/*连续图像运动变化处理*/
				key_pic_mp3(2);
			 }
			} //end else if
			  
		 }

		sleep (2);
		 	 
	  }//end while
	  close (cnt_fd);
	  close (pic_fd);
	  close (buttons_fd);
	exit(0);	
}
