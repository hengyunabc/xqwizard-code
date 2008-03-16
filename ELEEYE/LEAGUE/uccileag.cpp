/*
UCCILEAG - a Computer Chinese Chess League (UCCI Engine League) Emulator
Designed by Morning Yellow, Version: 3.62, Last Modified: Nov. 2007
Copyright (C) 2004-2007 www.elephantbase.net

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
#include "../utility/base.h"
#include "../utility/base2.h"
#include "../utility/parse.h"
#include "../utility/pipe.h"
#include "../utility/wsockbas.h"
#include "../utility/base64.h"
#include "../eleeye/position.h"
#include "../cchess/cchess.h"
#include "../cchess/ecco.h"
#include "../cchess/pgnfile.h"

const int MAX_CHAR = LINE_INPUT_MAX_CHAR; // ���뱨�������г��ȣ�ͬʱҲ�����淢�ͺͽ�����Ϣ������г���
const int MAX_ROBIN = 36;                 // ����ѭ��
const int MAX_TEAM = 32;                  // ���Ĳ�������
const int MAX_PROCESSORS = 32;            // ���Ĵ�������
const int QUEUE_LEN = 64;                 // ���������г���(����Ǵ�������������)

const char *const cszRobinChar = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

// �����ļ��ļ�¼�ṹ
struct CheckStruct {
  int mv, nTimer;
}; // chk

// �����ļ��Ŀ��ƽṹ
struct CheckFileStruct {
  FILE *fp;
  int nLen, nPtr;
  Bool Eof(void) {                    // �жϽ����ļ��Ƿ����
    return nPtr == nLen;
  }
  void Open(const char *szFileName);  // �򿪽����ļ�
  void Close(void) {                  // �رս����ļ�
    fclose(fp);
  }
  CheckStruct Read(void) {            // �������ļ��ļ�¼
    CheckStruct chkRecord;
    fseek(fp, nPtr * sizeof(CheckStruct), SEEK_SET);
    fread(&chkRecord, sizeof(CheckStruct), 1, fp);
    nPtr ++;
    return chkRecord;
  }
  void Write(CheckStruct chkRecord) { // д�����ļ��ļ�¼
    fseek(fp, nPtr * sizeof(CheckStruct), SEEK_SET);
    fwrite(&chkRecord, sizeof(CheckStruct), 1, fp);
    fflush(fp);
    nPtr ++;
    nLen ++;
  }
};

// �򿪽����ļ�������ļ������ڣ���ôҪ�½�һ�����ļ������´�
void CheckFileStruct::Open(const char *szFileName) {
  fp = fopen(szFileName, "r+b");
  if (fp == NULL) {
    fp = fopen(szFileName, "wb");
    if (fp == NULL) {
      printf("�����޷��������������ļ�\"%s\"!\n", szFileName);
      exit(EXIT_FAILURE);
    }
    fclose(fp);
    fp = fopen(szFileName, "r+b");
    if (fp == NULL) {
      printf("�����޷��򿪱��������ļ�\"%s\"!\n", szFileName);
      exit(EXIT_FAILURE);
    }
    nLen = nPtr = 0;
  } else {
    fseek(fp, 0, SEEK_END);
    nLen = ftell(fp) / sizeof(CheckStruct);
    nPtr = 0;
  }
}

// �����ӽṹ
struct TeamStruct {
  uint32 dwAbbr;
  int nEloValue, nKValue;
  char szEngineName[MAX_CHAR], szEngineFile[MAX_CHAR], szOptionFile[MAX_CHAR], szUrl[MAX_CHAR];
  int nWin, nDraw, nLoss, nScore;
};

// �������б�
static TeamStruct TeamList[MAX_TEAM];

// ����ȫ�ֱ���
static struct {
  int nTeamNum, nRobinNum, nRoundNum, nGameNum, nRemainProcs;
  int nInitTime, nIncrTime, nStopTime, nStandardCpuTime, nNameLen;
  Bool bPromotion;
  char szEvent[MAX_CHAR], szSite[MAX_CHAR];
  EccoApiStruct EccoApi;
} League;

// ѭ��������ͼ
static char RobinTable[2 * MAX_TEAM - 2][MAX_TEAM / 2][2];

// ֱ��ȫ�ֱ���
static struct {
  sint8 cResult[MAX_ROBIN][2 * MAX_TEAM - 2][MAX_TEAM / 2];
  char szHost[MAX_CHAR], szPath[MAX_CHAR], szPassword[MAX_CHAR];
  char szExt[MAX_CHAR], szJavaXQ[MAX_CHAR], szCounter[MAX_CHAR], szHeader[MAX_CHAR], szFooter[MAX_CHAR];
  char szProxyHost[MAX_CHAR], szProxyUser[MAX_CHAR], szProxyPassword[MAX_CHAR];
  int nPort, nRefresh, nInterval, nProxyPort;
  TimerStruct tb;
} Live;

static const char *const cszContent1 =
    "--[UCCI-LIVE-UPLOAD-BOUNDARY]" "\r\n"
    "Content-Disposition: form-data; name=\"upload\"; filename=\"upload.txt\"" "\r\n"
    "Content-Type: text/plain" "\r\n"
    "\r\n";
static const char *const cszContentFormat2 =
    "\r\n"
    "--[UCCI-LIVE-UPLOAD-BOUNDARY]" "\r\n"
    "Content-Disposition: form-data; name=\"filename\"" "\r\n"
    "\r\n"
    "%s" "\r\n"
    "--[UCCI-LIVE-UPLOAD-BOUNDARY]" "\r\n"
    "Content-Disposition: form-data; name=\"password\"" "\r\n"
    "\r\n"
    "%s" "\r\n"
    "--[UCCI-LIVE-UPLOAD-BOUNDARY]--" "\r\n";
static const char *const cszPostFormat =
    "POST %s HTTP/1.1" "\r\n"
    "Content-Type: multipart/form-data; boundary=[UCCI-LIVE-UPLOAD-BOUNDARY]" "\r\n"
    "Host: %s:%d" "\r\n"
    "Content-Length: %d" "\r\n"
    "\r\n";
static const char *const cszProxyFormat =
    "POST http://%s:%d%s HTTP/1.1" "\r\n"
    "Content-Type: multipart/form-data; boundary=[UCCI-LIVE-UPLOAD-BOUNDARY]" "\r\n"
    "Host: %s:%d" "\r\n"
    "Content-Length: %d" "\r\n"
    "\r\n";
static const char *const cszAuthFormat =
    "POST http://%s:%d%s HTTP/1.1" "\r\n"
    "Content-Type: multipart/form-data; boundary=[UCCI-LIVE-UPLOAD-BOUNDARY]" "\r\n"
    "Host: %s:%d" "\r\n"
    "Content-Length: %d" "\r\n"
    "Proxy-Authorization: Basic %s" "\r\n"
    "\r\n";

static void BlockSend(int nSocket, const char *lpBuffer, int nLen, int nTimeOut) {
  int nBytesWritten, nOffset;
  TimerStruct tb;

  nOffset = 0;
  tb.Init();
  while (nLen > 0 && tb.GetTimer() < nTimeOut) {
    nBytesWritten = WSBSend(nSocket, lpBuffer + nOffset, nLen);
    if (nBytesWritten == 0) {
      Idle();
    } else if (nBytesWritten < 0) {
      return;
    }
    nOffset += nBytesWritten;
    nLen -= nBytesWritten;
  }
}

const Bool FORCE_PUBLISH = TRUE;

static void HttpUpload(const char *szFileName) {
  FILE *fpUpload;
  int nSocket, nContentLen1, nFileLen, nContentLen2, nPostLen;
  char szPost[MAX_CHAR * 4], szContent2[MAX_CHAR * 4], szAuth[MAX_CHAR], szAuthB64[MAX_CHAR];

  fpUpload = fopen(szFileName, "rb");
  if (fpUpload == NULL) {
    return;
  }
  if (Live.nProxyPort == 0) {
    nSocket = WSBConnect(Live.szHost, Live.nPort);
  } else {
    nSocket = WSBConnect(Live.szProxyHost, Live.nProxyPort);
  }
  if (nSocket == INVALID_SOCKET) {
    fclose(fpUpload);
    return;
  }
  fseek(fpUpload, 0, SEEK_END);
  nContentLen1 = strlen(cszContent1);
  nFileLen = ftell(fpUpload);
  nContentLen2 = sprintf(szContent2, cszContentFormat2, szFileName, Live.szPassword);
  if (Live.nProxyPort == 0) {
    nPostLen = sprintf(szPost, cszPostFormat, Live.szPath, Live.szHost, Live.nPort, nContentLen1 + nFileLen + nContentLen2);
  } else {
    if (Live.szProxyUser[0] == '\0') {
      nPostLen = sprintf(szPost, cszProxyFormat, Live.szHost, Live.nPort, Live.szPath, Live.szHost, Live.nPort, nContentLen1 + nFileLen + nContentLen2);
    } else {
      nPostLen = sprintf(szAuth, "%s:%s", Live.szProxyUser, Live.szProxyPassword);
      B64Enc(szAuthB64, szAuth, nPostLen, 0);
      nPostLen = sprintf(szPost, cszAuthFormat, Live.szHost, Live.nPort, Live.szPath, Live.szHost, Live.nPort, nContentLen1 + nFileLen + nContentLen2, szAuthB64);
    }
  }
  // ��������ʽ�������ݣ���ʱΪ10�룬���ﻺ���������16K����������С��1.6KB/sʱ�����׳���
  BlockSend(nSocket, szPost, nPostLen, 10000);
  BlockSend(nSocket, cszContent1, nContentLen1, 10000);
  fseek(fpUpload, 0, SEEK_SET);
  while (nFileLen > 0) {
    nPostLen = MIN(nFileLen, MAX_CHAR * 4);
    fread(szPost, nPostLen, 1, fpUpload);
    BlockSend(nSocket, szPost, nPostLen, 10000);
    nFileLen -= nPostLen;
  }
  BlockSend(nSocket, szContent2, nContentLen2, 10000);
  WSBDisconnect(nSocket);
  fclose(fpUpload);
}

static const char *const cszResultDigit[4] = {
  "-", "(1-0)", "(1/2-1/2)", "(0-1)"
};

static Bool SkipUpload(Bool bForce) {
  if (Live.tb.GetTimer() < Live.nInterval) {
    // ������ϴ��ϴ����̫������ô�ݻ��ϴ�
    if (!bForce) {
      return TRUE;
    }
    // ���ǿ���ϴ�����ô����ȴ�
    while (Live.tb.GetTimer() < Live.nInterval) {
      Idle();
    }
  }
  Live.tb.Init();
  return FALSE;
}

static void PrintFile(FILE *fp, const char *szFileName) {
  char szLineStr[MAX_CHAR];
  char *lp;
  FILE *fpEmbedded;
  fpEmbedded = fopen(szFileName, "rt");
  if (fpEmbedded != NULL) {
    while (fgets(szLineStr, MAX_CHAR, fpEmbedded) != NULL) {
      lp = strchr(szLineStr, '\n');
      if (lp != NULL) {
        *lp = '\0';
      }
      fprintf(fp, "%s\n", szLineStr);
    }
    fclose(fpEmbedded);
  }
}

static void PublishLeague(void) {
  int nSortList[MAX_TEAM];
  int i, j, k, nLastRank ,nLastScore, nResult;
  uint32 dwHome, dwAway;
  TeamStruct *lpTeam;
  char szEmbeddedFile[MAX_CHAR];
  char szUploadFile[16];
  FILE *fp;

  SkipUpload(FORCE_PUBLISH); // ʼ�շ��� FALSE
  if (Live.nPort == 0) {
    return;
  }
  strcpy(szUploadFile, "index.");
  strncpy(szUploadFile + 6, Live.szExt, 6);
  szUploadFile[12] = '\0';
  fp = fopen(szUploadFile, "wt");
  if (fp == NULL) {
    return;
  }

  // ��ʾҳü
  fprintf(fp, "<html>\n");
  fprintf(fp, "  <head>\n");
  fprintf(fp, "    <meta name=\"GENERATOR\" content=\"UCCI������������ֱ��ϵͳ\">\n");
  fprintf(fp, "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=gb_2312-80\">\n");
  fprintf(fp, "    <title>%s ����ֱ��</title>\n", League.szEvent);
  fprintf(fp, "  </head>\n");
  fprintf(fp, "  <body background=\"background.gif\">\n");
  fprintf(fp, "    <p align=\"center\">\n");
  fprintf(fp, "      <font size=\"6\" face=\"����\">%s ����ֱ��</font>\n", League.szEvent);
  fprintf(fp, "    </p>\n");
  if (Live.szHeader[0] != '\0') {
    LocatePath(szEmbeddedFile, Live.szHeader);
    PrintFile(fp, szEmbeddedFile);
  }
  fprintf(fp, "    <p align=\"center\">\n");
  fprintf(fp, "      <font size=\"4\" face=\"����_GB2312\">\n");
  fprintf(fp, "        <strong>����</strong>\n");
  fprintf(fp, "      </font>\n");
  fprintf(fp, "    </p>\n");
  fprintf(fp, "    <table align=\"center\" border=\"1\">\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <th>����</th>\n");
  fprintf(fp, "        <th>��д</th>\n");
  fprintf(fp, "        <th>��������</th>\n");
  fprintf(fp, "        <th>�ȼ���</th>\n");
  fprintf(fp, "        <th>Kֵ</th>\n");
  fprintf(fp, "        <th>����</th>\n");
  fprintf(fp, "        <th>ʤ</th>\n");
  fprintf(fp, "        <th>��</th>\n");
  fprintf(fp, "        <th>��</th>\n");
  fprintf(fp, "        <th>����</th>\n");
  fprintf(fp, "      </tr>\n");

  // ��ʾ�������ɲ���"PrintRankList()"
  for (i = 0; i < League.nTeamNum; i ++) {
    nSortList[i] = i;
  }
  for (i = 0; i < League.nTeamNum - 1; i ++) {
    for (j = League.nTeamNum - 1; j > i; j --) {
      if (TeamList[nSortList[j - 1]].nScore < TeamList[nSortList[j]].nScore) {
        SWAP(nSortList[j - 1], nSortList[j]);
      }
    }
  }
  nLastRank = nLastScore = 0;
  for (i = 0; i < League.nTeamNum; i ++) {
    lpTeam = TeamList + nSortList[i];
    if (lpTeam->nScore != nLastScore) {
      nLastRank = i;
      nLastScore = lpTeam->nScore;
    }
    fprintf(fp, "      <tr>\n");
    fprintf(fp, "        <td align=\"center\">%d</td>\n", nLastRank + 1);
    fprintf(fp, "        <td align=\"center\">%.3s</td>\n", (const char *) &lpTeam->dwAbbr);
    fprintf(fp, "        <td align=\"center\">\n");
    if (lpTeam->szUrl[0] == '\0') {
      fprintf(fp, "          %s\n", lpTeam->szEngineName);
    } else {
      fprintf(fp, "          <a href=\"%s\" target=\"_blank\">%s</a>\n", lpTeam->szUrl, lpTeam->szEngineName);
    }
    fprintf(fp, "        </td>\n");
    fprintf(fp, "        <td align=\"center\">%d</td>\n", lpTeam->nEloValue);
    fprintf(fp, "        <td align=\"center\">%d</td>\n", lpTeam->nKValue);
    fprintf(fp, "        <td align=\"center\">%d</td>\n", lpTeam->nWin + lpTeam->nDraw + lpTeam->nLoss);
    fprintf(fp, "        <td align=\"center\">%d</td>\n", lpTeam->nWin);
    fprintf(fp, "        <td align=\"center\">%d</td>\n", lpTeam->nDraw);
    fprintf(fp, "        <td align=\"center\">%d</td>\n", lpTeam->nLoss);
    fprintf(fp, "        <td align=\"center\">%d%s</td>\n", lpTeam->nScore / 2, lpTeam->nScore % 2 == 0 ? "" : ".5");
    fprintf(fp, "      </tr>\n");
  }

  // ��ʾ����
  fprintf(fp, "    </table>\n");
  fprintf(fp, "    <p align=\"center\">\n");
  fprintf(fp, "      <font size=\"4\" face=\"����_GB2312\">\n");
  fprintf(fp, "        <strong>����</strong>\n");
  fprintf(fp, "      </font>\n");
  fprintf(fp, "    </p>\n");
  fprintf(fp, "    <table align=\"center\" border=\"1\">\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <th>�ִ�</th>\n");
  fprintf(fp, "        <th colspan=\"%d\">�Ծ�</th>\n", League.nGameNum);
  fprintf(fp, "      </tr>\n");

  // ��ʾ�Ծ�
  for (i = 0; i < League.nRobinNum; i ++) {
    for (j = 0; j < League.nRoundNum; j ++) {
      fprintf(fp, "      <tr>\n");
      fprintf(fp, "        <td align=\"center\">%d</td>\n", i * League.nRoundNum + j + 1);
      for (k = 0; k < League.nGameNum; k ++) {
        fprintf(fp, "        <td align=\"center\">\n");
        nResult = Live.cResult[i][j][k];
        dwHome = TeamList[RobinTable[j][k][0]].dwAbbr;
        dwAway = TeamList[RobinTable[j][k][1]].dwAbbr;
        if (nResult == -1) {
          fprintf(fp, "          %.3s-%.3s\n", (const char *) &dwHome, (const char *) &dwAway);
        } else {
          fprintf(fp, "          <a href=\"%.3s-%.3s%c.%s\" target=\"_blank\">\n", (const char *) &dwHome, (const char *) &dwAway, cszRobinChar[i], Live.szExt);
          if (nResult == 0) {
            fprintf(fp, "            <font color=\"#FF0000\">\n");
            fprintf(fp, "              <strong>\n");
          }
          fprintf(fp, "                %.3s%s%.3s\n", (const char *) &dwHome, cszResultDigit[nResult], (const char *) &dwAway);
          if (nResult == 0) {
            fprintf(fp, "              </strong>\n");
            fprintf(fp, "            </font>\n");
          }
          fprintf(fp, "          </a>\n");
        }
        fprintf(fp, "        </td>\n");
      }
      fprintf(fp, "      </tr>\n");
    }
  }

  // ��ʾҳ��
  fprintf(fp, "    </table>\n");
  fprintf(fp, "    <table>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">����</td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "    </table>\n");
  if (Live.szFooter[0] != '\0') {
    LocatePath(szEmbeddedFile, Live.szFooter);
    PrintFile(fp, szEmbeddedFile);
  }
  fprintf(fp, "    <table align=\"center\">\n");
  if (Live.szCounter[0] != '\0') {
    fprintf(fp, "      <tr>\n");
    fprintf(fp, "        <td align=\"center\">\n");
    fprintf(fp, "          <font face=\"����_GB2312\">\n");
    fprintf(fp, "            <strong>���ǵ�</strong>\n");
    fprintf(fp, "          </font>\n");
    fprintf(fp, "          <font face=\"Arial\">\n");
    fprintf(fp, "            <strong>\n");
    fprintf(fp, "              <script language=\"JavaScript\" src=\"%s\"></script>\n", Live.szCounter);
    fprintf(fp, "            </strong>\n");
    fprintf(fp, "          </font>\n");
    fprintf(fp, "          <font face=\"����_GB2312\">\n");
    fprintf(fp, "            <strong>λ����</strong>\n");
    fprintf(fp, "          </font>\n");
    fprintf(fp, "        </td>\n");
    fprintf(fp, "      </tr>\n");
    fprintf(fp, "      <tr>\n");
    fprintf(fp, "        <td align=\"center\">����</td>\n");
    fprintf(fp, "      </tr>\n");
  }
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <font size=\"2\">\n");
  fprintf(fp, "            ��ҳ���ɡ�<a href=\"http://www.elephantbase.net/league/emulator.htm\" target=\"_blank\">UCCI������������ֱ��ϵͳ</a>������\n");
  fprintf(fp, "          </font>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <a href=\"http://www.elephantbase.net/\" target=\"_blank\">\n");
  fprintf(fp, "            <img src=\"elephantbase.gif\" border=\"0\">\n");
  fprintf(fp, "          </a>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <a href=\"http://www.elephantbase.net/\" target=\"_blank\">\n");
  fprintf(fp, "            <font size=\"2\" face=\"Arial\">\n");
  fprintf(fp, "              <strong>www.elephantbase.net</strong>\n");
  fprintf(fp, "            </font>\n");
  fprintf(fp, "          </a>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "    </table>\n");
  fprintf(fp, "  </body>\n");
  fprintf(fp, "</html>\n");
  fclose(fp);
  HttpUpload(szUploadFile);
}

static const char *const cszResultChin[4] = {
  "��", "��ʤ", "�Ⱥ�", "�ȸ�"
};

inline void MOVE_ICCS(char *szIccs, int mv) {
  szIccs[0] = (FILE_X(SRC(mv))) + 'A' - FILE_LEFT;
  szIccs[1] = '9' + RANK_TOP - (RANK_Y(SRC(mv)));
  szIccs[2] = '-';
  szIccs[3] = (FILE_X(DST(mv))) + 'A' - FILE_LEFT;
  szIccs[4] = '9' + RANK_TOP - (RANK_Y(DST(mv)));
  szIccs[5] = '\0';
}

static void PublishGame(PgnFileStruct *lppgn, const char *szGameFile, Bool bForce = FALSE) {
  int i, mv, nStatus, nCounter;
  uint64 dqChinMove;
  char szEmbeddedFile[MAX_CHAR];
  char szUploadFile[16];
  char szIccs[8];
  char *lp;
  FILE *fp;
  PositionStruct pos;

  if (SkipUpload(bForce)) {
    return;
  }
  if (Live.nPort == 0) {
    return;
  }
  strcpy(szUploadFile, szGameFile);
  lp = strchr(szUploadFile, '.') + 1;
  strncpy(lp, Live.szExt, 6);
  lp[6] = '\0';
  fp = fopen(szUploadFile, "wt");
  if (fp == NULL) {
    return;
  }

  // ��ʾҳü
  fprintf(fp, "<html>\n");
  fprintf(fp, "  <head>\n");
  fprintf(fp, "    <meta name=\"GENERATOR\" content=\"UCCI������������ֱ��ϵͳ\">\n");
  fprintf(fp, "    <meta http-equiv=\"Content-Type\" content=\"text/html; charset=gb_2312-80\">\n");
  if (lppgn->nResult == 0 && Live.nRefresh != 0) {
    fprintf(fp, "    <meta http-equiv=\"Refresh\" content=\"%d;url=%s\">\n", Live.nRefresh, szUploadFile);
  }
  fprintf(fp, "    <title>%s (%s) %s - %s ����ֱ��</title>\n", lppgn->szRed, cszResultChin[lppgn->nResult], lppgn->szBlack, League.szEvent);
  fprintf(fp, "  </head>\n");
  fprintf(fp, "  <body background=\"background.gif\">\n");
  fprintf(fp, "    <p align=\"center\">\n");
  fprintf(fp, "      <font size=\"6\" face=\"����\">%s ����ֱ��</font>\n", League.szEvent);
  fprintf(fp, "    </p>\n");
  if (Live.szHeader[0] != '\0') {
    LocatePath(szEmbeddedFile, Live.szHeader);
    PrintFile(fp, szEmbeddedFile);
  }
  fprintf(fp, "    <p align=\"center\">\n");
  fprintf(fp, "      <font size=\"4\" face=\"����_GB2312\">\n");
  fprintf(fp, "        <strong>%s (%s) %s</strong>\n", lppgn->szRed, cszResultChin[lppgn->nResult], lppgn->szBlack);
  fprintf(fp, "      </font>\n");
  fprintf(fp, "    </p>\n");
  if (lppgn->nResult == 0) {
    fprintf(fp, "    <p align=\"center\">\n");
    fprintf(fp, "      <font size=\"2\">\n");
    fprintf(fp, "        <a href=\"%s\">\n", szUploadFile);
    fprintf(fp, "          <strong>�Ծֽ����У�������������û���Զ���ת����������</strong>\n");
    fprintf(fp, "        </a>\n");
    fprintf(fp, "      </font>\n");
    fprintf(fp, "    </p>\n");
  }

  // ��ʾJavaXQҳ��
  fprintf(fp, "    <table align=\"center\">\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <font size=\"2\">�ڷ� %s (%s)</font>\n", lppgn->szBlack, lppgn->szBlackElo);
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <applet code=\"JavaXQ\" codebase=\"%s\" align=\"baseline\" width=\"249\" height=\"301\">\n", Live.szJavaXQ);
  fprintf(fp, "            <param name=\"MoveList\" value=\"");
  for (i = 1; i <= lppgn->nMaxMove; i ++) {
    MOVE_ICCS(szIccs, lppgn->wmvMoveTable[i]);
    fprintf(fp, "%s ", szIccs);
  }
  fprintf(fp, "\">\n");
  if (lppgn->nResult == 0) {
    fprintf(fp, "            <param name=\"Step\" value=\"%d\">\n", lppgn->nMaxMove);
  }
  fprintf(fp, "          </applet>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <font size=\"2\">�췽 %s (%s)</font>\n", lppgn->szRed, lppgn->szRedElo);
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "    </table>\n");

  // ��ʾ������Ϣ
  if (lppgn->szEcco[0] == '\0') {
    fprintf(fp, "    <table align=\"center\">\n");
    fprintf(fp, "      <tr>\n");
    fprintf(fp, "        <td align=\"center\">����</td>\n");
    fprintf(fp, "      </tr>\n");
    fprintf(fp, "    </table>\n");
  } else {
    fprintf(fp, "    <p align=\"center\">\n");
    fprintf(fp, "      <font size=\"4\">\n");
    if (lppgn->szVar[0] == '\0') {
      fprintf(fp, "        <strong>%s(%s)</strong>\n", lppgn->szOpen, lppgn->szEcco);
    } else {
      fprintf(fp, "        <strong>%s����%s(%s)</strong>\n", lppgn->szOpen, lppgn->szVar, lppgn->szEcco);
    }
    fprintf(fp, "      </font>\n");
    fprintf(fp, "    </p>\n");
  }
  fprintf(fp, "    <table align=\"center\">\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td>\n");
  fprintf(fp, "          <dl>\n");

  // ��ʾ�ŷ�
  pos.FromFen(cszStartFen);
  nCounter = 1;
  for (i = 1; i <= lppgn->nMaxMove; i ++) {
    dqChinMove = File2Chin(Move2File(lppgn->wmvMoveTable[i], pos), pos.sdPlayer);
    if (pos.sdPlayer == 0) {
      fprintf(fp, "            <dt>%d. %.8s", nCounter, &dqChinMove);
    } else {
      fprintf(fp, " %.8s</dt>\n", &dqChinMove);
      nCounter ++;
    }
    TryMove(pos, nStatus, lppgn->wmvMoveTable[i]);
    if (pos.nMoveNum == MAX_MOVE_NUM) {
      pos.SetIrrev();
    }
  }
  if (pos.sdPlayer == 1) {
    fprintf(fp, "</dt>\n");
  }
  if (lppgn->szCommentTable[lppgn->nMaxMove] != NULL) {
    fprintf(fp, "            <dt>����%s</dt>\n", lppgn->szCommentTable[lppgn->nMaxMove]);
  }

  // ��ʾҳ��
  fprintf(fp, "          </dl>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <a href=\"%s\"><img src=\"pgn.gif\" border=\"0\">�������</a>", szGameFile);
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "    </table>\n");
  fprintf(fp, "    <dl>\n");
  fprintf(fp, "      <dt>����������Ѿ���װ��������ʦ����������ô����������ӣ���������ʦ���ͻ��Զ�����֡�</dt>\n");
  fprintf(fp, "      <dt>������������ʦ������������������Է�������ҳ�棬����ٶ������������ӣ�</dt>\n");
  fprintf(fp, "      <dt>����(1) ��������ʦ���������İ�</dt>\n");
  fprintf(fp, "      <dt>��������<a href=\"http://www.skycn.net/soft/24665.html\" target=\"_blank\">http://www.skycn.net/soft/24665.html</a>(�������վ)</dt>\n");
  fprintf(fp, "      <dt>��������<a href=\"http://www.onlinedown.net/soft/38287.htm\" target=\"_blank\">http://www.onlinedown.net/soft/38287.htm</a>(��������԰)</dt>\n");
  fprintf(fp, "      <dt>����(2) ��������ʦ���������İ�</dt>\n");
  fprintf(fp, "      <dt>��������<a href=\"http://www.skycn.net/soft/35392.html\" target=\"_blank\">http://www.skycn.net/soft/35392.html</a>(�������վ)</dt>\n");
  fprintf(fp, "      <dt>��������<a href=\"http://www.onlinedown.net/soft/47666.htm\" target=\"_blank\">http://www.onlinedown.net/soft/47666.htm</a>(��������԰)</dt>\n");
  fprintf(fp, "    </dl>\n");
  fprintf(fp, "    <ul>\n");
  fprintf(fp, "      <li>���ء�<a href=\"index.%s\">%s ����ֱ��</a></li>\n", Live.szExt, League.szEvent);
  fprintf(fp, "    </ul>\n");
  if (Live.szFooter[0] != '\0') {
    LocatePath(szEmbeddedFile, Live.szFooter);
    PrintFile(fp, szEmbeddedFile);
  }
  fprintf(fp, "    <table align=\"center\">\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <font size=\"2\">\n");
  fprintf(fp, "            ��ҳ���ɡ�<a href=\"http://www.elephantbase.net/league/emulator.htm\" target=\"_blank\">UCCI������������ֱ��ϵͳ</a>������\n");
  fprintf(fp, "          </font>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <a href=\"http://www.elephantbase.net/\" target=\"_blank\">\n");
  fprintf(fp, "            <img src=\"elephantbase.gif\" border=\"0\">\n");
  fprintf(fp, "          </a>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "      <tr>\n");
  fprintf(fp, "        <td align=\"center\">\n");
  fprintf(fp, "          <a href=\"http://www.elephantbase.net/\" target=\"_blank\">\n");
  fprintf(fp, "            <font size=\"2\" face=\"Arial\">\n");
  fprintf(fp, "              <strong>www.elephantbase.net</strong>\n");
  fprintf(fp, "            </font>\n");
  fprintf(fp, "          </a>\n");
  fprintf(fp, "        </td>\n");
  fprintf(fp, "      </tr>\n");
  fprintf(fp, "    </table>\n");
  fprintf(fp, "  </body>\n");
  fprintf(fp, "</html>\n");
  fclose(fp);
  HttpUpload(szUploadFile);
  // ���ڽ����ž��ϴ������ܱ�֤����ļ�һ���ϴ��ɹ�
  HttpUpload(szGameFile);
}

// �����ṹ��0��������(���з�)��1�����Ͷ�(���з�)
struct GameStruct {
  int sd, nCounter, nResult, nTimer[2];
  Bool bTimeout, bStarted[2], bUseMilliSec[2], bDraw;
  TimerStruct tbTimer;
  TeamStruct *lpTeam[2];
  PipeStruct pipe[2];
  PositionStruct posIrrev;
  char szIrrevFen[MAX_CHAR];
  char szGameFile[16];
  PgnFileStruct *lppgn;
  uint32 dwFileMove[20];
  FILE *fpLogFile;
  CheckFileStruct CheckFile;

  void Send(const char *szLineStr) {
    pipe[sd].LineOutput(szLineStr);
    fprintf(fpLogFile, "Emu->%.3s(%08d):%s\n", &lpTeam[sd]->dwAbbr, nTimer[sd] - tbTimer.GetTimer(), szLineStr);
    fflush(fpLogFile);
  }
  Bool Receive(char *szLineStr) {
    if (pipe[sd].LineInput(szLineStr)) {
      fprintf(fpLogFile, "%.3s->Emu(%08d):%s\n", &lpTeam[sd]->dwAbbr, nTimer[sd] - tbTimer.GetTimer(), szLineStr);
      fflush(fpLogFile);
      return TRUE;
    } else {
      return FALSE;
    }
  }
  void AddMove(int mv);  // ��һ���ŷ�
  void RunEngine(void);  // ����������
  void BeginGame(int nRobin, int nRound, int nGame); // ��ʼһ�����
  void ResumeGame(void); // �����ϴι�������
  Bool EndGame(int nRobin, int nRound, int nGame);   // ��ֹһ�����
};

static const char *const cszColorStr[2] = {
  "�췽", "�ڷ�"
};

const int BESTMOVE_THINKING = 0; // ��������˼����û�з���ֵ
const int BESTMOVE_DRAW = -1;    // ���������͵ķ���ֵ
const int BESTMOVE_RESIGN = -2;  // ��������ķ���ֵ
const int BESTMOVE_TIMEOUT = -3; // ���泬ʱ�ķ���ֵ

// ��һ���ŷ�
void GameStruct::AddMove(int mv) {
  int nStatus;
  uint32 dwEccoIndex;
  char *szComment;
  if (mv < BESTMOVE_THINKING) {
    szComment = new char[MAX_CHAR];
    lppgn->szCommentTable[lppgn->nMaxMove] = szComment;
    if (mv == BESTMOVE_DRAW) {
      strcpy(szComment, "˫�����");
      nResult = 2;
    } else {
      sprintf(szComment, mv == BESTMOVE_RESIGN ? "%s����" : "%s��ʱ����", cszColorStr[sd]);
      nResult = 3 - sd * 2;
    }
  } else {
    // ���ȰѸ��ŷ���¼��������
    lppgn->nMaxMove ++;
    lppgn->wmvMoveTable[lppgn->nMaxMove] = mv;
    // ����ECCO
    if (League.EccoApi.Available()) {
      if (lppgn->nMaxMove <= 20) {
        dwFileMove[lppgn->nMaxMove - 1] = Move2File(mv, posIrrev);
      }
      dwEccoIndex = League.EccoApi.EccoIndex((const char *) dwFileMove);
      strcpy(lppgn->szEcco, (const char *) &dwEccoIndex);
      strcpy(lppgn->szOpen, League.EccoApi.EccoOpening(dwEccoIndex));
      strcpy(lppgn->szVar, League.EccoApi.EccoVariation(dwEccoIndex));
    }
    // Ȼ��������ŷ������ж�״̬
    TryMove(posIrrev, nStatus, mv);
    if ((nStatus & MOVE_CAPTURE) != 0) {
      posIrrev.ToFen(szIrrevFen);
      posIrrev.SetIrrev();
    }
    nTimer[sd] += League.nIncrTime * League.nStandardCpuTime;
    if (nStatus < MOVE_MATE) {
      // ����������ŷ�����ô�����Ϊ�������С�
      nResult = 0;
    } else {
      // �������ֹ�ŷ�����ô����״̬�ж����
      szComment = new char[MAX_CHAR];
      lppgn->szCommentTable[lppgn->nMaxMove] = szComment;
      if (FALSE) {
      } else if ((nStatus & MOVE_ILLEGAL) != 0 || (nStatus & MOVE_INCHECK) != 0) {
        sprintf(szComment, "%s�߷�Υ��", cszColorStr[sd]);
        nResult = 3 - sd * 2;
      } else if ((nStatus & MOVE_DRAW) != 0) {
        strcpy(szComment, "������Ȼ��������");
        nResult = 2;
      } else if ((nStatus & MOVE_PERPETUAL) != 0) {
        if ((nStatus & MOVE_PERPETUAL_WIN) != 0) {
          if ((nStatus & MOVE_PERPETUAL_LOSS) != 0) {
            strcpy(szComment, "˫����������");
            nResult = 2;
          } else {
            sprintf(szComment, "%s��������", cszColorStr[1 - sd]);
            nResult = 1 + sd * 2;
          }
        } else {
          if ((nStatus & MOVE_PERPETUAL_LOSS) != 0) {
            sprintf(szComment, "%s��������", cszColorStr[sd]);
            nResult = 3 - sd * 2;
          } else {
            strcpy(szComment, "˫����������");
            nResult = 2;
          }
        }
      } else { // MOVE_MATE
        sprintf(szComment, "%sʤ", cszColorStr[sd]);
        nResult = 1 + sd * 2;
      }
    }
  }
  lppgn->nResult = nResult;
  // �������ӷ�����ʵ"sd"��"posIrrev.sdPlayer"��ͬ����
  sd = 1 - sd;
}

const char *const cszGo = "go time %d increment %d opptime %d oppincrement %d";
const char *const cszGoDraw = "go draw time %d increment %d opptime %d oppincrement %d";

// ����������
void GameStruct::RunEngine(void) {
  char szLineStr[MAX_CHAR], szFileName[MAX_CHAR];
  char *lpLineChar;
  int i, nStatus, nMoveNum, nBanMoves;
  int mvBanList[MAX_GEN_MOVES];
  MoveStruct mvs[MAX_GEN_MOVES];
  uint32 dwMoveStr;
  FILE *fpOptionFile;

  if (!bStarted[sd]) {
    // ���������δ��������ô��������
    tbTimer.Init();
    LocatePath(szFileName, lpTeam[sd]->szEngineFile);
    pipe[sd].Open(szFileName);
    Send("ucci");
    // ����"ucci"ָ�����10�����ڵȴ�"ucciok"������Ϣ
    while (tbTimer.GetTimer() < 10000) {
      if (Receive(szLineStr)) {
        if (StrEqv(szLineStr, "option usemillisec ")) {
          bUseMilliSec[sd] = TRUE;
        }
        if (StrEqv(szLineStr, "ucciok")) {
          break;
        }
      } else {
        Idle();
      }
    }
    // ���ñ�Ҫ�ĳ�ʼ��ѡ��
    if (League.bPromotion) {
      Send("setoption promotion true");
    } else {
      Send("setoption promotion false");
    }
    Send("setoption ponder false");
    Send("setoption newgame");
    if (bUseMilliSec[sd]) {
      Send("setoption usemillisec true");
    }
    // ������ѡ���ļ������ݷ��͸�����
    LocatePath(szFileName, lpTeam[sd]->szOptionFile);
    fpOptionFile = fopen(szFileName, "rt");
    if (fpOptionFile != NULL) {
      while (fgets(szLineStr, MAX_CHAR, fpOptionFile) != NULL) {
        lpLineChar = strchr(szLineStr, '\n');
        if (lpLineChar != NULL) {
          *lpLineChar = '\0';
        }
        Send(szLineStr);
      }
      fclose(fpOptionFile);
    }
    bStarted[sd] = TRUE;
  }

  // �����淢�͵�ǰ����
  tbTimer.Init();
  lpLineChar = szLineStr;
  lpLineChar += sprintf(lpLineChar, "position fen %s - - 0 1", szIrrevFen);
  if (posIrrev.nMoveNum > 1) {
    lpLineChar += sprintf(lpLineChar, " moves");
    for (i = 1; i < posIrrev.nMoveNum; i ++) {
      dwMoveStr = MOVE_COORD(posIrrev.rbsList[i].mvs.wmv);
      lpLineChar += sprintf(lpLineChar, " %.4s", (const char *) &dwMoveStr);
    }
  }
  Send(szLineStr);

  // �����淢�ͽ���ָ��
  nBanMoves = 0;
  nMoveNum = posIrrev.GenAllMoves(mvs);
  for (i = 0; i < nMoveNum; i ++) {
    if (posIrrev.MakeMove(mvs[i].wmv)) {
      // ������˺����ŷ��������ɳ��򲢴ﵽ�����ظ�������ŷ�������Ϊ����
      // ע�⣺�����Ѿ��ֵ��Է������ˣ�����"REP_WIN"�ű�ʾ����
      if (posIrrev.RepStatus(2) == REP_WIN) {
        mvBanList[nBanMoves] = mvs[i].wmv;
        nBanMoves ++;
      }
      posIrrev.UndoMakeMove();
    }
  }
  if (nBanMoves > 0) {
    lpLineChar = szLineStr;
    lpLineChar += sprintf(lpLineChar, "banmoves");
    for (i = 0; i < nBanMoves; i ++) {
      dwMoveStr = MOVE_COORD(mvBanList[i]);
      lpLineChar += sprintf(lpLineChar, " %.4s", (const char *) &dwMoveStr);
    }
    Send(szLineStr);
  }

  // �����淢������ָ�"go [draw] time %d increment %d opptime %d oppincrement %d";
  if (bUseMilliSec[sd]) {
    sprintf(szLineStr, bDraw ? cszGoDraw : cszGo, nTimer[sd],
        League.nIncrTime * League.nStandardCpuTime, nTimer[1 - sd], League.nIncrTime * League.nStandardCpuTime);
  } else {
    sprintf(szLineStr, bDraw ? cszGoDraw : cszGo, nTimer[sd] / 1000,
        League.nIncrTime * League.nStandardCpuTime / 1000, nTimer[1 - sd] / 1000, League.nIncrTime * League.nStandardCpuTime / 1000);
  }
  Send(szLineStr);
  bTimeout = FALSE;
}

// ��ʼһ�����
void GameStruct::BeginGame(int nRobin, int nRound, int nGame) {
  int i;
  char szFileName[16];
  CheckStruct chkRecord;
  time_t dwTime;
  tm *lptm;

  Live.cResult[nRobin][nRound][nGame] = 0;
  PublishLeague();
  League.nRemainProcs --; // ��ʣ����ô�����������һ
  time(&dwTime);
  lptm = localtime(&dwTime); // ���ʱ��
  lpTeam[0] = TeamList + RobinTable[nRound][nGame][0];
  lpTeam[1] = TeamList + RobinTable[nRound][nGame][1];
  sd = nCounter = nResult = 0;
  nTimer[0] = nTimer[1] = League.nInitTime * League.nStandardCpuTime * 60;
  bStarted[0] = bStarted[1] = bUseMilliSec[0] = bUseMilliSec[1] = bDraw = FALSE;
  strcpy(szIrrevFen, cszStartFen);
  posIrrev.FromFen(szIrrevFen);

  // �ϳ������ļ�
  lppgn = new PgnFileStruct();
  lppgn->posStart = posIrrev;
  sprintf(szGameFile, "%.3s-%.3s%c.PGN", (const char *) &lpTeam[0]->dwAbbr, (const char *) &lpTeam[1]->dwAbbr, cszRobinChar[nRobin]);
  strcpy(lppgn->szEvent, League.szEvent);
  sprintf(lppgn->szRound, "%d", nRobin * League.nRoundNum + nRound + 1);
  sprintf(lppgn->szDate, "%04d.%02d.%02d", lptm->tm_year + 1900, lptm->tm_mon + 1, lptm->tm_mday);
  strcpy(lppgn->szSite, League.szSite);
  strcpy(lppgn->szRed, lpTeam[0]->szEngineName);
  sprintf(lppgn->szRedElo, "%d", lpTeam[0]->nEloValue);
  strcpy(lppgn->szBlack, lpTeam[1]->szEngineName);
  sprintf(lppgn->szBlackElo, "%d", lpTeam[1]->nEloValue);
  for (i = 0; i < 20; i ++) {
    dwFileMove[i] = 0;
  }

  // ����־�ļ��ͽ����ļ�
  sprintf(szFileName, "%.3s-%.3s%c.LOG", (const char *) &lpTeam[0]->dwAbbr, (const char *) &lpTeam[1]->dwAbbr, cszRobinChar[nRobin]);
  fpLogFile = fopen(szFileName, "at");
  if (fpLogFile == NULL) {
    printf("�����޷�������־�ļ�\"%s\"!\n", szFileName);
    exit(EXIT_FAILURE);
  }
  sprintf(szFileName, "%.3s-%.3s%c.CHK", (const char *) &lpTeam[0]->dwAbbr, (const char *) &lpTeam[1]->dwAbbr, cszRobinChar[nRobin]);
  CheckFile.Open(szFileName);

  // ��������ļ��м�¼����ô�Ƚ������̼�¼���ŷ�
  while (!CheckFile.Eof()) {
    chkRecord = CheckFile.Read();
    nTimer[sd] = chkRecord.nTimer;
    AddMove(chkRecord.mv);
    if (nResult > 0) {
      lppgn->Write(szGameFile);
      PublishGame(lppgn, szGameFile, FORCE_PUBLISH);
      League.nRemainProcs ++; // ��EndGame()֮ǰ���ͷŴ�������Դ����ߴ�����������
      return; // �����ֽ���(�����ļ���������)����ô�Ͳ�������������
    }
  }
  lppgn->Write(szGameFile);
  PublishGame(lppgn, szGameFile);
  RunEngine(); // �����ļ������(�����ļ�������)������Ҫ����������
  // �����淢��ָ�����־͹��𣬵ȴ��´ε���"ResumeGame()"�Լ�������
}

// �����ϴι�������
void GameStruct::ResumeGame(void) {
  char szLineStr[MAX_CHAR];
  CheckStruct chkRecord;
  char *lp;

  if (nResult == 0) { // �����δ����ʱ���в���
    // ���ȶ�ȡ���淴����Ϣ
    chkRecord.mv = BESTMOVE_THINKING;
    while (Receive(szLineStr)) {
      lp = szLineStr;
      if (StrEqvSkip(lp, "bestmove ")) {
        chkRecord.mv = COORD_MOVE(*(uint32 *) lp);
        lp += 4;
        if (StrScan(lp, " resign")) {
          chkRecord.mv = BESTMOVE_RESIGN;
        } else {
          if (StrScan(lp, " draw")) {
            if (bDraw) {
              chkRecord.mv = BESTMOVE_DRAW;
            } else {
              bDraw = TRUE;
            }
          } else {
            bDraw = FALSE;
          };
        };
        break;
      } else if (StrEqv(lp, "nobestmove")) {
        chkRecord.mv = BESTMOVE_RESIGN;
        break;
      }
    }
    // ���û�ж��������ŷ������ж������Ƿ�ʱ
    if (chkRecord.mv == BESTMOVE_THINKING) {
      if (bTimeout) {
        if (tbTimer.GetTimer() > nTimer[sd] + League.nStopTime) {
          chkRecord.mv = BESTMOVE_TIMEOUT; // ֻ��ʱ�䳬��ֹͣʱ��󣬲Ÿ������Ա�ʾ��ʱ
        }
      } else {
        if (tbTimer.GetTimer() > nTimer[sd]) {
          Send("stop");
          bTimeout = TRUE;
        }
      }
    }
    // ����з����ŷ�(������ʱ���صĿ���)����������ŷ�
    if (chkRecord.mv != BESTMOVE_THINKING) {
      nTimer[sd] -= tbTimer.GetTimer();
      if (nTimer[sd] < 0) {
        nTimer[sd] = 0;
      }
      chkRecord.nTimer = nTimer[sd];
      CheckFile.Write(chkRecord);
      AddMove(chkRecord.mv);
      lppgn->Write(szGameFile);
      PublishGame(lppgn, szGameFile, nResult > 0);
      if (nResult == 0) {
        RunEngine(); // ��������δ��������ô������˼����һ����
      } else {
        // �������Ѿ���������ô��ֹ��������
        for (sd = 0; sd < 2; sd ++) {
          if (bStarted[sd]) {
            tbTimer.Init();
            Send("quit");
            while (tbTimer.GetTimer() < 1000) {
              if (Receive(szLineStr)) {
                if (StrEqv(szLineStr, "bye")) {
                  break;
                }
              } else {
                Idle();
              }
            }
            pipe[sd].Close();
          }
        }
        League.nRemainProcs ++; // ��EndGame()֮ǰ���ͷŴ�������Դ����ߴ�����������
      }
    }
  }
}

const struct ResultStruct {
  int nHomeWin, nHomeDraw, nHomeLoss, nHomeScore, nAwayWin, nAwayDraw, nAwayLoss, nAwayScore;
  double dfWHome;
  const char *szResultStr;
} ResultList[4] = {
  {0, 0, 0, 0, 0, 0, 0, 0, 0.0, "       "},
  {1, 0, 0, 2, 0, 0, 1, 0, 1.0, "  1-0  "},
  {0, 1, 0, 1, 0, 1, 0, 1, 0.5, "1/2-1/2"},
  {0, 0, 1, 0, 1, 0, 0, 2, 0.0, "  0-1  "}
};

inline void PrintDup(int nChar, int nDup) {
  int i;
  for (i = 0; i < nDup; i ++) {
    putchar(nChar);
  }
}

// ��ֹһ�����
Bool GameStruct::EndGame(int nRobin, int nRound, int nGame) {
  char szLineStr[MAX_CHAR];
  double dfWeHome;
  const ResultStruct *lpResult;

  if (nResult > 0) {
    delete lppgn;
    fclose(fpLogFile);
    CheckFile.Close();
    // �������Ѿ���ɣ���ô����ɼ�
    dfWeHome = 1.0 / (1.0 + pow(10.0, (double) (lpTeam[1]->nEloValue - lpTeam[0]->nEloValue) / 400.0));
    lpResult = ResultList + nResult;
    lpTeam[0]->nWin += lpResult->nHomeWin;
    lpTeam[0]->nDraw += lpResult->nHomeDraw;
    lpTeam[0]->nLoss += lpResult->nHomeLoss;
    lpTeam[0]->nScore += lpResult->nHomeScore;
    lpTeam[1]->nWin += lpResult->nAwayWin;
    lpTeam[1]->nDraw += lpResult->nAwayDraw;
    lpTeam[1]->nLoss += lpResult->nAwayLoss;
    lpTeam[1]->nScore += lpResult->nAwayScore;
    lpTeam[0]->nEloValue += (int) ((lpResult->dfWHome - dfWeHome) * lpTeam[0]->nKValue);
    lpTeam[1]->nEloValue += (int) ((dfWeHome - lpResult->dfWHome) * lpTeam[1]->nKValue);
    // ÿ�ֵĵ�һ������ʾ�ִ�
    if (nGame == 0) {
      printf("�� %d �֣�\n\n", nRobin * League.nRoundNum + nRound + 1);
    }
    // �����ֽ��
    printf("%s", lpTeam[0]->szEngineName);
    PrintDup(' ', League.nNameLen - strlen(lpTeam[0]->szEngineName));
    printf(" %s %s", lpResult->szResultStr, lpTeam[1]->szEngineName);
    PrintDup(' ', League.nNameLen - strlen(lpTeam[1]->szEngineName));
    printf(" (%.3s-%.3s%c.PGN)\n", (const char *) &lpTeam[0]->dwAbbr, (const char *) &lpTeam[1]->dwAbbr, cszRobinChar[nRobin]);
    fflush(stdout);
    // ����ֱ��
    Live.cResult[nRobin][nRound][nGame] = nResult;
    PublishLeague();
    return TRUE;
  } else {
    return FALSE;
  }
}

// ���������
static void PrintRankList(void) {
  int i, j, k, nLastRank, nLastScore;
  int nSortList[MAX_TEAM];
  TeamStruct *lpTeam;
  // �����ͷ
  printf("   ��д ��������");
  PrintDup(' ', League.nNameLen - 8);
  printf(" ELO  K   ʤ  ��  �� ����\n");
  PrintDup('=', League.nNameLen - 8);
  printf("================" "==========================\n");
  for (i = 0; i < League.nTeamNum; i ++) {
    nSortList[i] = i;
  }
  // ��ð�����򷨰���������
  for (i = 0; i < League.nTeamNum - 1; i ++) {
    for (j = League.nTeamNum - 1; j > i; j --) {
      if (TeamList[nSortList[j - 1]].nScore < TeamList[nSortList[j]].nScore) {
        SWAP(nSortList[j - 1], nSortList[j]);
      }
    }
  }
  // ������ʾ���Σ�������ֺ�ǰһ������ͬ����ô����Ҳ��ǰһ������ͬ
  nLastRank = nLastScore = 0;
  for (i = 0; i < League.nTeamNum; i ++) {
    lpTeam = TeamList + nSortList[i];
    if (lpTeam->nScore != nLastScore) {
      nLastRank = i;
      nLastScore = lpTeam->nScore;
    }
    printf("%2d %.3s  %s", nLastRank + 1, (const char *) &lpTeam->dwAbbr, lpTeam->szEngineName);
    PrintDup(' ', League.nNameLen - strlen(lpTeam->szEngineName));
    printf(" %4d %2d %3d %3d %3d %3d%s\n", lpTeam->nEloValue, lpTeam->nKValue, lpTeam->nWin,
        lpTeam->nDraw, lpTeam->nLoss, lpTeam->nScore / 2, lpTeam->nScore % 2 == 0 ? "" : ".5");
  }
  PrintDup('=', League.nNameLen - 8);      
  printf("================" "==========================\n\n");
  //     "   ��д ��������" " ELO  K   ʤ  ��  �� ����"
}

// ��ֶ���ռ�ô����ռ䣬���Ա�����ȫ�ֱ���
static GameStruct GameList[QUEUE_LEN];

// ������
int main(void) {
  // ���±���ǣ�����뱨��Ķ�ȡ
  char szLineStr[MAX_CHAR];
  char *lp, *lpComma;
  FILE *fpIniFile;
  TeamStruct *lpTeam;
  int i, j, k, nSocket;
  int nEngineFileLen; // �����ļ�����󳤶�
  // ���±���ǣ�浽��ֶ��еĿ���
  int nRobinPush, nRoundPush, nGamePush;
  int nRobinPop, nRoundPop, nGamePop;
  int nQueueBegin, nQueueEnd, nQueueIndex;

  // �����Ƕ�ȡ���뱨��
  League.nTeamNum = League.nInitTime = League.nIncrTime = League.nStopTime = 0;
  League.nRemainProcs = League.nRobinNum = 1;
  League.nStandardCpuTime = 1000;
  League.nNameLen = nEngineFileLen = 8; // �������ƺ������ļ�����С����
  League.bPromotion = FALSE;
  League.szEvent[0] = League.szSite[0] = '\0';
  Live.szHost[0] = Live.szPath[0] = Live.szPassword[0] = Live.szCounter[0] = '\0';
  Live.szProxyHost[0] = Live.szProxyUser[0] = Live.szProxyPassword[0] = '\0';
  strcpy(Live.szExt, "htm");
  strcpy(Live.szJavaXQ, "http://www.elephantbase.net/java/");
  Live.nPort = Live.nProxyPort = 80;
  Live.nRefresh = Live.nInterval = 0;

  LocatePath(szLineStr, "UCCILEAG.INI");
  fpIniFile = fopen(szLineStr, "rt");
  if (fpIniFile == NULL) {
    printf("�����޷��������ļ�\"%s\"��\n", szLineStr);
    return 0;
  }
  while (fgets(szLineStr, MAX_CHAR, fpIniFile) != NULL) {
    StrCutCrLf(szLineStr);
    lp = szLineStr;
    if (FALSE) {
    } else if (StrEqvSkip(lp, "Event=")) {
      strcpy(League.szEvent, lp);
    } else if (StrEqvSkip(lp, "Site=")) {
      strcpy(League.szSite, lp);
    } else if (StrEqvSkip(lp, "Roundrobins=")) {
      League.nRobinNum = Str2Digit(lp, 1, MAX_ROBIN);
    } else if (StrEqvSkip(lp, "Processors=")) {
      League.nRemainProcs = Str2Digit(lp, 1, MAX_PROCESSORS);
    } else if (StrEqvSkip(lp, "InitialTime=")) {
      League.nInitTime = Str2Digit(lp, 1, 500);
    } else if (StrEqvSkip(lp, "IncrementalTime=")) {
      League.nIncrTime = Str2Digit(lp, 0, 500);
    } else if (StrEqvSkip(lp, "StoppingTime=")) {
      League.nStopTime = Str2Digit(lp, 0, 500);
    } else if (StrEqvSkip(lp, "StandardCpuTime=")) {
      League.nStandardCpuTime = Str2Digit(lp, 100, 10000);
    } else if (StrEqvSkip(lp, "Promotion=")) {
      if (StrEqv(lp, "True")) {
        League.bPromotion = TRUE;
      } else if (StrEqv(lp, "On")) {
        League.bPromotion = TRUE;
      }
    } else if (StrEqvSkip(lp, "Team=")) {
      if (League.nTeamNum < MAX_TEAM) {
        lpTeam = TeamList + League.nTeamNum;
        lpTeam->dwAbbr = *(uint32 *) lp;
        StrSplitSkip(lp, ',');
        StrSplitSkip(lp, ',', lpTeam->szEngineName);
        League.nNameLen = MAX(League.nNameLen, (int) strlen(lpTeam->szEngineName));
        lpTeam->nEloValue = Str2Digit(lp, 0, 9999);
        StrSplitSkip(lp, ',');        
        lpTeam->nKValue = Str2Digit(lp, 0, 99);
        StrSplitSkip(lp, ',');
        StrSplitSkip(lp, ',', lpTeam->szEngineFile);
        nEngineFileLen = MAX(nEngineFileLen, (int) strlen(lpTeam->szEngineFile));
        StrSplitSkip(lp, ',', lpTeam->szOptionFile);
        StrSplitSkip(lp, ',', lpTeam->szUrl);
        League.nTeamNum ++;
      }
    // ���²���ֻ��ת���й�
    } else if (StrEqvSkip(lp, "Host=")) {
      strcpy(Live.szHost, lp);     // ֱ������
    } else if (StrEqvSkip(lp, "Path=")) {
      strcpy(Live.szPath, lp);     // �ϴ�ҳ��·��
    } else if (StrEqvSkip(lp, "Password=")) {
      strcpy(Live.szPassword, lp); // �ϴ�ҳ�����
    } else if (StrEqvSkip(lp, "Extension=")) {
      strcpy(Live.szExt, lp);      // �ϴ��ļ���׺
    } else if (StrEqvSkip(lp, "JavaXQ=")) {
      strcpy(Live.szJavaXQ, lp);   // JavaXQ·��
    } else if (StrEqvSkip(lp, "Counter=")) {
      strcpy(Live.szCounter, lp);  // ������·��
    } else if (StrEqvSkip(lp, "Header=")) {
      strcpy(Live.szHeader, lp);   // ҳü·��
    } else if (StrEqvSkip(lp, "Footer=")) {
      strcpy(Live.szFooter, lp);   // ҳ��·��
    } else if (StrEqvSkip(lp, "Port=")) {
      Live.nPort = Str2Digit(lp, 1, 65535);      // HTTP�˿�
    } else if (StrEqvSkip(lp, "Refresh=")) {
      Live.nRefresh = Str2Digit(lp, 0, 60);      // ҳ���Զ�ˢ��ʱ��(��)
    } else if (StrEqvSkip(lp, "Interval=")) {
      Live.nInterval = Str2Digit(lp, 0, 60000);  // �ϴ��ļ����ʱ��(����)
    } else if (StrEqvSkip(lp, "ProxyHost=")) {
      strcpy(Live.szProxyHost, lp);              // ��������
    } else if (StrEqvSkip(lp, "ProxyPort=")) {
      Live.nProxyPort = Str2Digit(lp, 1, 65535); // �����˿�
    } else if (StrEqvSkip(lp, "ProxyUser=")) {
      strcpy(Live.szProxyUser, lp);              // �����û���
      Live.szProxyUser[1024] = '\0';
    } else if (StrEqvSkip(lp, "ProxyPassword=")) {
      strcpy(Live.szProxyPassword, lp);          // ��������
      Live.szProxyPassword[1024] = '\0';
    }
  }
  fclose(fpIniFile);
  if (League.nTeamNum < 2) {
    printf("����������Ҫ���������ӣ�\n");
    return 0;
  }
  printf("#======================#\n");
  printf("$ UCCI��������������� $\n");
  printf("#======================#\n\n");
  printf("���£�����%s\n", League.szEvent);
  printf("�ص㣺����%s\n", League.szSite);
  printf("����������%d\n", League.nTeamNum);
  printf("����������%d\n", League.nRemainProcs);
  printf("ѭ��������%d\n", League.nRobinNum);
  printf("��ʼʱ�䣺%-4d ����\n", League.nInitTime);
  printf("ÿ����ʱ��%-4d ��\n", League.nIncrTime);
  printf("ֹͣʱ�䣺%-4d ����\n", League.nStopTime);
  printf("���������%-4d ����\n", League.nStandardCpuTime);
  if (League.bPromotion) {
    printf("���򣺡���������(ʿ)��(��)����ɱ�(��)\n");
  }
  printf("ģ��������UCCI��������ģ���� 3.62\n\n");
  printf("�������棺\n\n");
  printf("   ��д ��������");
  PrintDup(' ', League.nNameLen - 8);
  printf(" ELO  K  �������");
  PrintDup(' ', nEngineFileLen - 8);
  printf(" �����ļ�\n");
  PrintDup('=', League.nNameLen + nEngineFileLen - 16);
  printf("================" "=================" "============\n");
  for (i = 0; i < League.nTeamNum; i ++) {
    lpTeam = TeamList + i;
    printf("%2d %.3s  %s", i + 1, (const char *) &lpTeam->dwAbbr, lpTeam->szEngineName);
    PrintDup(' ', League.nNameLen - strlen(lpTeam->szEngineName));
    printf(" %4d %2d %s", lpTeam->nEloValue, lpTeam->nKValue, lpTeam->szEngineFile);
    PrintDup(' ', nEngineFileLen - strlen(lpTeam->szEngineFile));
    printf(" %s\n", lpTeam->szOptionFile);
  }
  PrintDup('=', League.nNameLen + nEngineFileLen - 16);
  printf("================" "=================" "============\n\n");
  //     "   ��д ��������" " ELO  K  �����ļ�" " �����ļ�"

  // ����������ѭ������������ɲ��ģ�http://www.elephantbase.net/protocol/roundrobin.htm
  League.nGameNum = (League.nTeamNum + 1) / 2;
  League.nRoundNum = League.nGameNum * 2 - 1;
  for (i = 0; i < League.nGameNum; i ++) {
    RobinTable[0][i][0] = i;
    RobinTable[0][i][1] = League.nGameNum * 2 - 1 - i;
  }
  for (i = 1; i < League.nRoundNum; i ++) {
    RobinTable[i][0][1] = League.nGameNum * 2 - 1;
    for (j = 0; j < League.nGameNum - 1; j ++) {
      RobinTable[i][j][0] = RobinTable[i - 1][League.nGameNum - 1 - j][1];
      RobinTable[i][j + 1][1] = RobinTable[i - 1][League.nGameNum - 1 - j][0];
    }
    RobinTable[i][League.nGameNum - 1][0] = RobinTable[i - 1][0][0];
  }
  if (League.nTeamNum % 2 == 0) {
    for (i = 0; i < League.nRoundNum; i ++) {
      if (i % 2 != 0) {
        SWAP(RobinTable[i][0][0], RobinTable[i][0][1]);
      }
    }
  } else {
    League.nGameNum --;
    for (i = 0; i < League.nRoundNum; i ++) {
      for (j = 0; j < League.nGameNum; j ++) {
        RobinTable[i][j][0] = RobinTable[i][j + 1][0];
        RobinTable[i][j][1] = RobinTable[i][j + 1][1];
      }
    }
  }
  for (i = 0; i < League.nRoundNum; i ++) {
    for (j = 0; j < League.nGameNum; j ++) {
      RobinTable[League.nRoundNum + i][j][0] = RobinTable[i][j][1];
      RobinTable[League.nRoundNum + i][j][1] = RobinTable[i][j][0];
    }
  }
  League.nRoundNum *= 2;
  printf("���̱���\n\n");
  printf("�ִ� �Ծ�\n");
  printf("=====");
  for (i = 0; i < League.nGameNum; i ++) {
    printf("========");
  }
  printf("\n");
  for (i = 0; i < League.nRobinNum; i ++) {
    for (j = 0; j < League.nRoundNum; j ++) {
      printf("%3d ", i * League.nRoundNum + j + 1);
      for (k = 0; k < League.nGameNum; k ++) {
        printf(" %.3s-%.3s", (const char *) &TeamList[RobinTable[j][k][0]].dwAbbr, (const char *) &TeamList[RobinTable[j][k][1]].dwAbbr);
        Live.cResult[i][j][k] = -1;
      }
      printf("\n");
    }
  }
  printf("=====");
  for (i = 0; i < League.nGameNum; i ++) {
    printf("========");
  }
  printf("\n\n");

  // ��ʼ��ECCO��������
  LocatePath(szLineStr, cszLibEccoFile);
  League.EccoApi.Startup(szLineStr);

  // ����ֱ��ҳ��
  WSBStartup();
  nSocket = (Live.szProxyHost[0] == '\0' ? INVALID_SOCKET : WSBConnect(Live.szProxyHost, Live.nProxyPort));
  if (nSocket == INVALID_SOCKET) {
    Live.nProxyPort = 0;
    nSocket = (Live.szHost[0] == '\0' ? INVALID_SOCKET : WSBConnect(Live.szHost, Live.nPort));
    if (nSocket == INVALID_SOCKET) {
      Live.nPort = 0;
    } else {
      WSBDisconnect(nSocket);
    }
  } else {
    WSBDisconnect(nSocket);
  }
  Live.tb.Init();

  // ���ڿ�ʼ������ֶ��У����Ǳ�ģ�����ĺ��Ĳ���
  printf("=== �������̿�ʼ ===\n\n");
  fflush(stdout);
  PreGenInit();
  ChineseInit();
  PreEval.bPromotion = League.bPromotion;
  nRobinPush = nRoundPush = nGamePush = 0; // ѹ����е�ѭ�����ִκ�������
  nRobinPop = nRoundPop = nGamePop = 0;    // �������е�ѭ�����ִκ�������
  nQueueBegin = nQueueEnd = 0;             // ���г��ںͶ������
  while (nRobinPop < League.nRobinNum) {
    // ��һ�����ѹ����е������ǣ�(1) ���б�����ɣ�(2) ��ʣ��Ĵ�������(3) ����δ������
    if (nRobinPush < League.nRobinNum && League.nRemainProcs > 0 && (nQueueEnd + 1) % QUEUE_LEN != nQueueBegin) {
      GameList[nQueueEnd].BeginGame(nRobinPush, nRoundPush, nGamePush);
      nQueueEnd = (nQueueEnd + 1) % QUEUE_LEN;
      // �Ѱ�һ�����ѹ����У��޸�ѭ�����ִκ�������
      nGamePush ++;
      if (nGamePush == League.nGameNum) {
        nGamePush = 0;
        nRoundPush ++;
        if (nRoundPush == League.nRoundNum) {
          nRoundPush = 0;
          nRobinPush ++;
        }
      }
    }

    // ���ȶ����е�ÿ����֣��൱������ת��ʽ�����������
    nQueueIndex = nQueueBegin;
    while (nQueueIndex != nQueueEnd) {
      GameList[nQueueIndex].ResumeGame();
      nQueueIndex = (nQueueIndex + 1) % QUEUE_LEN;
    }

    // ������в��ǿյģ���ô���Խ���ֵ�������
    if (nQueueBegin != nQueueEnd) {
      if (GameList[nQueueBegin].EndGame(nRobinPop, nRoundPop, nGamePop)) {
        nQueueBegin = (nQueueBegin + 1) % QUEUE_LEN;
        // �Ѱ�һ����ֵ������У��޸�ѭ�����ִκ�������
        nGamePop ++;
        if (nGamePop == League.nGameNum) {
          // ���һ�ֽ�������ô���������
          printf("\n");
          printf("�� %d �ֺ�������\n\n", nRobinPop * League.nRoundNum + nRoundPop + 1);
          PrintRankList();
          fflush(stdout);
          nGamePop = 0;
          nRoundPop ++;
          if (nRoundPop == League.nRoundNum) {
            nRoundPop = 0;
            nRobinPop ++;
          }
        }
      }
    }
    Idle();
  }
  printf("=== �������̽��� ===\n\n");
  printf("����������\n\n");
  PrintRankList();

  WSBCleanup();
  League.EccoApi.Shutdown();
  return 0;
}