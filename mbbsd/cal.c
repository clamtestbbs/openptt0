/* $Id: cal.c,v 1.7 2000/09/13 06:23:53 davidyu Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include "config.h"
#include "pttstruct.h"
#include "common.h"
#include "modes.h"
#include "perm.h"
#include "proto.h"

extern struct utmpfile_t *utmpshm;
extern int usernum;

/* 防堵 Multi play */
static int count_multiplay(int unmode) {
    register int i, j;
    register userinfo_t *uentp;
    extern struct utmpfile_t *utmpshm;

    for(i = j = 0; i < USHM_SIZE; i++) {
	uentp = &(utmpshm->uinfo[i]);
	if(uentp->uid == usernum)
	    if(uentp->lockmode == unmode)
		j++;
    }
    return j;
}

extern userinfo_t *currutmp;
extern char *ModeTypeTable[];

int lockutmpmode(int unmode, int state) {
    int errorno = 0;
    
    if(currutmp->lockmode)
	errorno = 1;
    else if(count_multiplay(unmode))
	errorno = 2;
    
    if(errorno && !(state == LOCK_THIS && errorno == LOCK_MULTI)) {
	clear();
	move(10,20);
	if(errorno == 1)
	    prints("請先離開 %s 才能再 %s ",
		   ModeTypeTable[currutmp->lockmode],
		   ModeTypeTable[unmode]);
	else 
	    prints("抱歉! 您已有其他線相同的ID正在%s",
		   ModeTypeTable[unmode]);
	pressanykey();
	return errorno;
    }
    
    setutmpmode(unmode);
    currutmp->lockmode = unmode;
    return 0;
}

int unlockutmpmode() {
    currutmp->lockmode = 0;
    return 0;
}

extern userec_t cuser;
extern userec_t xuser;

/* 使用錢的函數 */
int reload_money() {
    passwd_query(usernum, &xuser);
    cuser.money = xuser.money;
    cuser.lastsong = xuser.lastsong;
    return 0;
}

#define VICE_NEW   "vice.new"
#define VICE_BASE  "etc/vice.base"
#define VICE_COUNT "etc/vice.count"

/* Heat:發票 */
static int vice(int money, char* item) {
    char genbuf[200], buf[128];
    fileheader_t mymail;
    int viceserial, base = 0;
    /* bfp就是要用shm來取代的檔案唷 */
    FILE *bfp = fopen(VICE_BASE, "r+"),*cfp=fopen(VICE_COUNT,"a"),
        *nfp;
    
    sprintf(buf, BBSHOME"/home/%c/%s/%s",
	    cuser.userid[0], cuser.userid, VICE_NEW);
    nfp = fopen(buf, "a");
    if(!bfp || !nfp || !cfp)
	return 0;
   
    if(fgets(buf, 9, bfp))
	base = atoi(buf);
    else
	perror("can't open bingo file of vice file");
    viceserial = ++base;
    
    fprintf(cfp,"%s\n",cuser.userid);
    fprintf(nfp, "%08d\n", viceserial);
    rewind(bfp);
    fprintf(bfp, "%08d", base);
    fclose(bfp);
    fclose(nfp);
    fclose(cfp);
   
    sprintf(genbuf, BBSHOME "/home/%c/%s", cuser.userid[0], cuser.userid);
    stampfile(genbuf, &mymail);
    strcpy(mymail.owner, BBSNAME "經濟部");
    sprintf(mymail.title, "%s 花了%d$ \033[1;37m編號[\033[1;33m%08d\033[m]",
	    item, money, viceserial);
    mymail.savemode = 0;
    unlink(genbuf);
    Link(BBSHOME "/etc/vice.txt", genbuf);   
    sprintf(genbuf,BBSHOME"/home/%c/%s/.DIR", cuser.userid[0], cuser.userid);
    
    append_record(genbuf, &mymail, sizeof(mymail));
    return 0;
}

#define lockreturn(unmode, state) if(lockutmpmode(unmode, state)) return 
#define lockreturn0(unmode, state) if(lockutmpmode(unmode, state)) return 0
#define lockbreak(unmode, state) if(lockutmpmode(unmode, state)) break
#define SONGBOOK  "etc/SONGBOOK"
#define OSONGPATH "etc/SONGO"
extern char trans_buffer[];
extern char save_title[];

static int osong(char *defaultid) {
    char destid[IDLEN + 1],buf[200],genbuf[200],filename[256],say[51];
    char receiver[60],ano[2];
    FILE *fp,*fp1;// *fp2;
    fileheader_t mail;
//, mail2;
    time_t now;
    
    now = time(NULL);
    strcpy(buf, Cdatedate(&now));
    reload_money();
    
    lockreturn0(OSONG, LOCK_MULTI);
    
    reload_money();
    /* Jaky 一人一天點一首 */
    if(!strcmp(buf, Cdatedate(&cuser.lastsong)) && !HAS_PERM(PERM_SYSOP)) {
	move(22,0);
	outs("你今天已經點過囉，明天再點吧....");
	refresh();
	pressanykey();

	unlockutmpmode();
	return 0;
    }

    if(cuser.money < 200) {
	move(22, 0);
	outs("點歌要200銀唷!....");
	refresh();
	pressanykey();
	unlockutmpmode();
	return 0;
    }
    move(12, 0);
    clrtobot();
    sprintf(buf, "親愛的 %s 歡迎來到歐桑自動點歌系統\n", cuser.userid);
    outs(buf);
    trans_buffer[0] = 0;
    if(!defaultid){
	  getdata(13, 0, "要點給誰呢:[可直接按 Enter 先選歌]", destid, IDLEN + 1, DOECHO);
	  while (!destid[0]){
	     a_menu("點歌歌本", SONGBOOK,0 );
	     clear();
	     getdata(13, 0, "要點給誰呢:[可按 Enter 重新選歌]", destid, IDLEN + 1, DOECHO);
	  }
	}
    else
	strcpy(destid,defaultid);

    /* Heat:點歌者匿名功能 */    
    getdata(14,0, "要匿名嗎?[y/n]:", ano, 2, DOECHO);
     
    if(!destid[0]) {
	unlockutmpmode();
	return 0;
    }
    
    getdata_str(14, 0, "想要要對他(她)說..:", say, 51, DOECHO, "我愛妳..");
    sprintf(save_title, "%s:%s", (ano[0]=='y')?"匿名者":cuser.userid, say);
    getdata_str(16, 0, "寄到誰的信箱(可用E-mail)?", receiver, 45,
		LCECHO, destid);
    

    
    if (!trans_buffer[0]){
       outs("\n接著要選歌囉..進入歌本好好的選一首歌吧..^o^");
       pressanykey();    
       a_menu("點歌歌本", SONGBOOK,0 );
    }
    if(!trans_buffer[0] ||  strstr(trans_buffer, "home") ||
       strstr(trans_buffer, "boards") || !(fp = fopen(trans_buffer, "r"))) {
	unlockutmpmode();
	return 0;
    }
    
    strcpy(filename, OSONGPATH);
    
    stampfile(filename, &mail);
    
    unlink(filename);
    
    if(!(fp1 = fopen(filename, "w"))) {
	fclose(fp);
	unlockutmpmode();
	return 0;
    }
    
    strcpy(mail.owner, "點歌機");
    sprintf(mail.title, "◇ %s 點給 %s ", (ano[0]=='y')?"匿名者":cuser.userid, destid);
    mail.savemode = 0;
    
    while(fgets(buf, 200, fp)) {
	char *po;
	if(!strncmp(buf, "標題: ", 6)) {
	    clear();
	    move(10,10);prints("%s", buf);
	    pressanykey();
	    fclose(fp);
	    unlockutmpmode();
	    return 0;
	}
	while((po = strstr(buf, "<~Src~>"))) {
	    po[0] = 0;
	    sprintf(genbuf,"%s%s%s",buf,(ano[0]=='y')?"匿名者":cuser.userid,po+7);
	    strcpy(buf,genbuf);
        }
	while((po = strstr(buf, "<~Des~>"))) {
	    po[0] = 0;
	    sprintf(genbuf,"%s%s%s",buf,destid,po+7);
	    strcpy(buf,genbuf);
        }
	while((po = strstr(buf, "<~Say~>"))) {
	    po[0] = 0;
	    sprintf(genbuf,"%s%s%s",buf,say,po+7);
	    strcpy(buf,genbuf);
        }
	fputs(buf,fp1);
    }
    fclose(fp1);
    fclose(fp);

//    do_append(OSONGMAIL "/.DIR", &mail2, sizeof(mail2));
    
    if(do_append(OSONGPATH "/.DIR", &mail, sizeof(mail)) != -1) {
	  cuser.lastsong = time(NULL);
	/* Jaky 超過 500 首歌就開始砍 */
	  if (get_num_records(OSONGPATH "/.DIR", sizeof(mail)) > 500){
	    delete_record(OSONGPATH "/.DIR", sizeof(mail), 1);
	  }
	/* 把第一首拿掉 */
	  demoney(200);
    }
    sprintf(save_title, "%s:%s", (ano[0]=='y')?"匿名者":cuser.userid, say);
    hold_mail(filename, destid);

    if(receiver[0]) {
#ifndef USE_BSMTP
	bbs_sendmail(filename, save_title, receiver);
#else
	bsmtp(filename, save_title, receiver,0);
#endif
    }
    clear();
    outs(
	"\n\n  恭喜您點歌完成囉..\n"
	"  一小時內動態看板會自動重新更新\n"
	"  大家就可以看到您點的歌囉\n\n"
	"  點歌有任何問題可以到Note板的精華區找答案\n"
	"  也可在Note板精華區看到自己的點歌記錄\n"
	"  有任何保貴的意見也歡迎到Note板留話\n"
	"  讓親切的板主為您服務\n");
    pressanykey();
    vice(200, "點歌");
    sortsong();
    topsong();

    unlockutmpmode();
    return 1;
}

int ordersong() {
    osong(NULL);
    return 0;
}

/* 使用錢的函數 */
int inumoney(char *tuser, int money) {
    int unum;
    
    if((unum = getuser(tuser))) {
	xuser.money += money;
	passwd_update(unum, &xuser);
	return xuser.money;
    } else
	return -1;
}

int inmoney(int money) {
    passwd_query(usernum, &xuser);
    cuser.money = xuser.money + money;
    passwd_update(usernum, &cuser);
    return cuser.money;
}

static int inmailbox(int m) {
    passwd_query(usernum, &xuser);
    cuser.exmailbox = xuser.exmailbox + m;
    passwd_update(usernum, &cuser);
    return cuser.exmailbox;
}

int deumoney(char *tuser, int money) {
    int unum;
    if((unum = getuser(tuser))) {
	if((unsigned long int)xuser.money <= (unsigned long int)money)
	    xuser.money=0;
	else
	    xuser.money -= money;
	passwd_update(unum, &xuser);
	return xuser.money;
    } else
	return -1;
}

int demoney(int money) {
    passwd_query(usernum, &xuser);
    if((unsigned long int)xuser.money <= (unsigned long int)money)
	cuser.money=0;
    else
	cuser.money = xuser.money - money;
    passwd_update(usernum, &cuser);
    return cuser.money;
}

extern int b_lines;

#if !HAVE_FREECLOAK
/* 花錢選單 */
int p_cloak() {
    char buf[4];
    getdata(b_lines-1, 0,
	    currutmp->invisible ? "確定要現身?[y/N]" : "確定要隱身?[y/N]",
	    buf, 3, LCECHO);
    if(buf[0] != 'y')
	return 0;
    if(cuser.money >= 19) {
	demoney(19);
	vice(19, "cloak");
	currutmp->invisible %= 2;
	outs((currutmp->invisible ^= 1) ? MSG_CLOAKED : MSG_UNCLOAK);
	refresh();
	safe_sleep(1);
    }
    return 0;
}
#endif

int p_from() {
    char ans[4];

    getdata(b_lines-2, 0, "確定要改故鄉?[y/N]", ans, 3, LCECHO);
    if(ans[0] != 'y')
	return 0;
    reload_money();
    if(cuser.money < 49)
	return 0;
    if(getdata_buf(b_lines-1, 0, "請輸入新故鄉:",
		   currutmp->from, 17, DOECHO)) {
	demoney(49);
	vice(49,"home");
	currutmp->from_alias=0;
    }
    return 0;
}

int p_exmail() {
    char ans[4],buf[200];
    int n;

    if(cuser.exmailbox >= MAX_EXKEEPMAIL) {
	sprintf(buf,"容量最多增加 %d 封，不能再買了。", MAX_EXKEEPMAIL);
	outs(buf);
	refresh();
	return 0;
    }
    sprintf(buf,"您曾增購 %d 封容量，還要再買多少?",
	    cuser.exmailbox);
    
    getdata_str(b_lines-2, 0, buf, ans, 3, LCECHO, "10");
    
    n = atoi(ans);
    if(!ans[0] || !n)
	return 0;
    if(n + cuser.exmailbox > MAX_EXKEEPMAIL)
	n = MAX_EXKEEPMAIL - cuser.exmailbox;
    reload_money();
    if(cuser.money < n * 1000)
	return 0;
    demoney(n * 1000);
    vice(n * 1000, "mail");
    inmailbox(n);
    return 0;
}

void mail_redenvelop(char* from, char* to, int money, char mode){
    char genbuf[200];
    fileheader_t fhdr;
    time_t now;
    FILE* fp;
    sprintf(genbuf, "home/%c/%s", to[0], to);
    stampfile(genbuf, &fhdr);
    if (!(fp = fopen(genbuf, "w")))
        return;
    now = time(NULL);
    fprintf(fp, "作者: %s\n"
                "標題: 招財進寶\n"
                "時間: %s\n"
                "\033[1;33m親愛的 %s ：\n\n\033[m"
                "\033[1;31m    我包給你一個 %d 元的大紅包喔 ^_^\n\n"
                "    禮輕情意重，請笑納...... ^_^\033[m\n"
                , from, ctime(&now), to, money);
    fclose(fp);
    sprintf(fhdr.title, "招財進寶");
    strcpy(fhdr.owner, from);

    if (mode == 'y')
       vedit(genbuf, NA, NULL);
    sprintf(genbuf, "home/%c/%s/.DIR", to[0], to);       
    append_record(genbuf, &fhdr, sizeof(fhdr));
}

int p_give() {
    int money;
    char id[IDLEN + 1], genbuf[90];
    time_t now = time(0);
    
    move(1,0);
    usercomplete("這位幸運兒的id:", id);
    if(!id[0] || !strcmp(cuser.userid,id) ||
       !getdata(2, 0, "要給多少錢:", genbuf, 7, LCECHO))
	return 0;
    money = atoi(genbuf);
    reload_money();
    if(money > 0 && cuser.money >= money && (inumoney(id, money) != -1)) {
	demoney(money);
	now = time(NULL);
	sprintf(genbuf,"%s\t給%s\t%d\t%s", cuser.userid, id, money,
		ctime(&now));
	log_file(FN_MONEY, genbuf);
	genbuf[0] = 'n';
	getdata(3, 0, "要自行書寫紅包袋嗎？[y/N]", genbuf, 2, LCECHO);
	mail_redenvelop(cuser.userid, id, money, genbuf[0]);
    }
    return 0;
}

int p_touch_boards() {
    touch_boards();
    move(b_lines - 1, 0);
    outs("BCACHE 更新完成\n");
    pressanykey();
    return 0;
}

int p_sysinfo() {
    char buf[100];
    long int total,used;
    float p;
    
    move(b_lines-1,0);
    clrtoeol();
#ifndef __CYGWIN__
    cpuload(buf);
#endif
    outs("CPU 負荷 : ");
    outs(buf);
    pressanykey();
    return 0;
}

/* 小計算機 */
static void ccount(float *a, float b, int cmode) {
    switch(cmode) {
    case 0:
    case 1:
    case 2:
        *a += b;
        break;
    case 3:
        *a -= b;
        break;
    case 4:
        *a *= b;
        break;
    case 5:
        *a /= b;
        break;
    }
}

int cal() {
    float a = 0;
    char flo = 0, ch = 0;
    char mode[6] = {' ','=','+','-','*','/'} , cmode = 0;
    char buf[100] = "[            0] [ ] ", b[20] = "0";
    
    move(b_lines - 1, 0);
    clrtoeol();
    outs(buf);
    move(b_lines, 0);
    clrtoeol();
    outs("\033[44m 小計算機  \033[31;47m      (0123456789+-*/=) "
	 "\033[30m輸入     \033[31m  "
	 "(Q)\033[30m 離開                   \033[m");
    while(1) {
	ch = igetch();
	switch(ch) {
	case '\r':
            ch = '=';
	case '=':
	case '+':
	case '-':
	case '*':
	case '/':
            ccount(&a, atof(b), cmode);
            flo = 0;
            b[0] = '0';
            b[1] = 0;
            move(b_lines - 1, 0);
            sprintf(buf, "[%13.2f] [%c] ", a, ch);
            outs(buf);
            break;
	case '.':
            if(!flo)
		flo = 1;
            else
		break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
	case '0':
	    if(strlen(b) > 13)
		break;
	    if(flo || b[0] != '0')
		sprintf(b,"%s%c",b,ch);
	    else
		b[0]=ch;
	    move(b_lines - 1, 0);
	    sprintf(buf, "[%13s] [%c]", b, mode[(int)cmode]);
	    outs(buf);
	    break;
	case 'q':
	    return 0;
	}
	
	switch(ch) {
	case '=':
	    a = 0;
	    cmode = 0;
	    break;
	case '+':
	    cmode = 2;
	    break;
	case '-':
	    cmode = 3;
	    break;
	case '*':
	    cmode = 4;
	    break;
	case '/':
	    cmode = 5;
	    break;
	}
    }
}
