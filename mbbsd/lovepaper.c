/* $Id: lovepaper.c,v 1.3 2000/09/13 06:23:54 davidyu Exp $ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include "config.h"
#include "pttstruct.h"
#include "modes.h"
#include "common.h"
#include "proto.h"

#define DATA "etc/lovepaper.dat"

extern userec_t cuser;

int x_love() {
    char buf1[200], save_title[TTLEN + 1];
    char receiver[61], path[STRLEN] = "home/";
    int x, y = 0, tline = 0, poem = 0;
    FILE *fp, *fpo;
    time_t timenow;
    struct tm *gtime;
    fileheader_t mhdr;
    
    setutmpmode(LOVE);
    time(&timenow);
    gtime = localtime(&timenow);
    sprintf(buf1,"%c/%s/love%d%d",
	    cuser.userid[0], cuser.userid,gtime->tm_sec,gtime->tm_min);
    strcat(path,buf1);
    move(1,0);
    clrtobot();
    
    outs("\n歡迎使用情書產生器 v0.00 版 \n");
    outs("有何難以啟齒的話,交由系統幫你說吧.\n爸爸說 : 濫情不犯法.\n");
    
    if(!getdata(7, 0, "收信人：", receiver, 60, DOECHO)) return 0;
    if(receiver[0] && !(searchuser(receiver) &&
			getdata(8, 0, "主  題：", save_title,
				TTLEN, DOECHO))) {
	move(10, 0);
	outs("收信人或主題不正確, 情書無法傳遞. ");
	pressanykey();
	return 0;
    }
    
    fpo = fopen(path, "w");
    fprintf(fpo, "\n");
    if((fp = fopen(DATA, "r"))) {
	while(fgets(buf1,100, fp)) {
	    switch(buf1[0]) {
	    case '#':
		break;
	    case '@':
		if(!strncmp(buf1, "@begin", 6) || !strncmp(buf1, "@end", 4))
		    tline=3;
		else if(!strncmp(buf1,"@poem",5)) {
		    poem = 1;
		    tline = 1;
		    fprintf(fpo, "\n\n");
		} else
		    tline=2;
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
		sscanf(buf1,"%d",&x);
		y = (rand() % (x - 1)) * tline; 
		break;
	    default:
		if(!poem) {
		    if(y > 0)
			y = y - 1;
		    else {
			if(tline > 0) {
			    fprintf(fpo, "%s", buf1);
			    tline--;
			}
		    }
		} else {
		    if(buf1[0] == '$')
			y--;
		    else if(y == 0)
			fprintf(fpo,"%s",buf1);
		}
	    } 

	}
	
	fclose(fp);
	fclose(fpo);
	if(vedit(path, YEA, NULL) == -1) {
	    unlink(path);
	    clear();
	    outs("\n\n 放棄寄情書\n");
	    pressanykey();
	    return -2;
	}
	sethomepath(buf1, receiver);
	stampfile(buf1, &mhdr);
	Rename(path, buf1);
	strncpy(mhdr.title, save_title, TTLEN);
	strcpy(mhdr.owner, cuser.userid);
	mhdr.savemode = '\0';
	sethomedir(path, receiver );
	if(append_record(path, &mhdr, sizeof(mhdr)) == -1)
	    return -1;
	hold_mail(buf1, receiver);
	return 1;
    }
    return 0;
}
