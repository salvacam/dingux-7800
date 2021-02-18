/*
 *  Copyright (C) 2006 Ludovic Jacomme (ludovic.jacomme@gmail.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "global.h"
#include "psp_atari.h"
#include "psp_sdl.h"
#include "psp_kbd.h"
#include "psp_joy.h"
#include "psp_menu.h"
#include "psp_fmgr.h"

  ATARI_t ATARI;

  int psp_screenshot_mode = 0;
  int psp_exit_now = 0;

static int 
loc_load_rom(char *TmpName)
{
  int error = psp_atari_load_rom(TmpName);
  return error;
}

static int 
loc_load_rom_buffer(char *rom_buffer, int rom_size)
{
  int error = psp_atari_load_rom_buffer(rom_buffer, rom_size);
  if (rom_buffer) free(rom_buffer);
  return error;
}

void
atari_update_save_name(char *Name)
{
  char        TmpFileName[MAX_PATH];
  struct stat aStat;
  int         index;
  char       *SaveName;
  char       *Scan1;
  char       *Scan2;

  SaveName = strrchr(Name,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = Name;

  if (!strncasecmp(SaveName, "sav_", 4)) {
    Scan1 = SaveName + 4;
    Scan2 = strrchr(Scan1, '_');
    if (Scan2 && (Scan2[1] >= '0') && (Scan2[1] <= '5')) {
      strncpy(ATARI.atari_save_name, Scan1, MAX_PATH);
      ATARI.atari_save_name[Scan2 - Scan1] = '\0';
    } else {
      strncpy(ATARI.atari_save_name, SaveName, MAX_PATH);
    }
  } else {
    strncpy(ATARI.atari_save_name, SaveName, MAX_PATH);
  }

  if (ATARI.atari_save_name[0] == '\0') {
    strcpy(ATARI.atari_save_name,"default");
  }

  for (index = 0; index < ATARI_MAX_SAVE_STATE; index++) {
    ATARI.atari_save_state[index].used  = 0;
    memset(&ATARI.atari_save_state[index].date, 0, sizeof(time_t));
    ATARI.atari_save_state[index].thumb = 0;

    snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.sta", ATARI.atari_home_dir, ATARI.atari_save_name, index);
    if (! stat(TmpFileName, &aStat)) {
      ATARI.atari_save_state[index].used = 1;
      ATARI.atari_save_state[index].date = aStat.st_mtime;
      snprintf(TmpFileName, MAX_PATH, "%s/save/sav_%s_%d.png", ATARI.atari_home_dir, ATARI.atari_save_name, index);
      if (! stat(TmpFileName, &aStat)) {
        if (psp_sdl_load_thumb_png(ATARI.atari_save_state[index].surface, TmpFileName)) {
          ATARI.atari_save_state[index].thumb = 1;
        }
      }
    }
  }

  ATARI.comment_present = 0;
  snprintf(TmpFileName, MAX_PATH, "%s/txt/%s.txt", ATARI.atari_home_dir, ATARI.atari_save_name);
  if (! stat(TmpFileName, &aStat)) {
    ATARI.comment_present = 1;
  }
}

void
reset_save_name()
{
  atari_update_save_name("");
}

typedef struct thumb_list {
  struct thumb_list *next;
  char              *name;
  char              *thumb;
} thumb_list;

static thumb_list* loc_head_thumb = 0;

static void
loc_del_thumb_list()
{
  while (loc_head_thumb != 0) {
    thumb_list *del_elem = loc_head_thumb;
    loc_head_thumb = loc_head_thumb->next;
    if (del_elem->name) free( del_elem->name );
    if (del_elem->thumb) free( del_elem->thumb );
    free(del_elem);
  }
}

static void
loc_add_thumb_list(char* filename)
{
  thumb_list *new_elem;
  char tmp_filename[MAX_PATH];

  strcpy(tmp_filename, filename);
  char* save_name = tmp_filename;

  /* .png extention */
  char* Scan = strrchr(save_name, '.');
  if ((! Scan) || (strcasecmp(Scan, ".png"))) return;
  *Scan = 0;

  if (strncasecmp(save_name, "sav_", 4)) return;
  save_name += 4;

  Scan = strrchr(save_name, '_');
  if (! Scan) return;
  *Scan = 0;

  /* only one png for a give save name */
  new_elem = loc_head_thumb;
  while (new_elem != 0) {
    if (! strcasecmp(new_elem->name, save_name)) return;
    new_elem = new_elem->next;
  }

  new_elem = (thumb_list *)malloc( sizeof( thumb_list ) );
  new_elem->next = loc_head_thumb;
  loc_head_thumb = new_elem;
  new_elem->name  = strdup( save_name );
  new_elem->thumb = strdup( filename );
}

void
load_thumb_list()
{
  char SaveDirName[MAX_PATH];
  DIR* fd = 0;

  loc_del_thumb_list();

  snprintf(SaveDirName, MAX_PATH, "%s/save", ATARI.atari_home_dir);

  fd = opendir(SaveDirName);
  if (!fd) return;

  struct dirent *a_dirent;
  while ((a_dirent = readdir(fd)) != 0) {
    if(a_dirent->d_name[0] == '.') continue;
    if (a_dirent->d_type != DT_DIR) 
    {
      loc_add_thumb_list( a_dirent->d_name );
    }
  }
  closedir(fd);
}

int
load_thumb_if_exists(char *Name)
{
  char        FileName[MAX_PATH];
  char        ThumbFileName[MAX_PATH];
  struct stat aStat;
  char       *SaveName;
  char       *Scan;

  strcpy(FileName, Name);
  SaveName = strrchr(FileName,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = FileName;

  Scan = strrchr(SaveName,'.');
  if (Scan) *Scan = '\0';

  if (!SaveName[0]) return 0;

  thumb_list *scan_list = loc_head_thumb;
  while (scan_list != 0) {
    if (! strcasecmp( SaveName, scan_list->name)) {
      snprintf(ThumbFileName, MAX_PATH, "%s/save/%s", ATARI.atari_home_dir, scan_list->thumb);
      if (! stat(ThumbFileName, &aStat)) {
        if (psp_sdl_load_thumb_png(save_surface, ThumbFileName)) {
          return 1;
        }
      }
    }
    scan_list = scan_list->next;
  }
  return 0;
}

typedef struct comment_list {
  struct comment_list *next;
  char              *name;
  char              *filename;
} comment_list;

static comment_list* loc_head_comment = 0;

static void
loc_del_comment_list()
{
  while (loc_head_comment != 0) {
    comment_list *del_elem = loc_head_comment;
    loc_head_comment = loc_head_comment->next;
    if (del_elem->name) free( del_elem->name );
    if (del_elem->filename) free( del_elem->filename );
    free(del_elem);
  }
}

static void
loc_add_comment_list(char* filename)
{
  comment_list *new_elem;
  char  tmp_filename[MAX_PATH];

  strcpy(tmp_filename, filename);
  char* save_name = tmp_filename;

  /* .png extention */
  char* Scan = strrchr(save_name, '.');
  if ((! Scan) || (strcasecmp(Scan, ".txt"))) return;
  *Scan = 0;

  /* only one txt for a given save name */
  new_elem = loc_head_comment;
  while (new_elem != 0) {
    if (! strcasecmp(new_elem->name, save_name)) return;
    new_elem = new_elem->next;
  }

  new_elem = (comment_list *)malloc( sizeof( comment_list ) );
  new_elem->next = loc_head_comment;
  loc_head_comment = new_elem;
  new_elem->name  = strdup( save_name );
  new_elem->filename = strdup( filename );
}

void
load_comment_list()
{
  char SaveDirName[MAX_PATH];
  DIR* fd = 0;

  loc_del_comment_list();

  snprintf(SaveDirName, MAX_PATH, "%s/txt", ATARI.atari_home_dir);

  fd = opendir(SaveDirName);
  if (!fd) return;

  struct dirent *a_dirent;
  while ((a_dirent = readdir(fd)) != 0) {
    if(a_dirent->d_name[0] == '.') continue;
    if (a_dirent->d_type != DT_DIR) 
    {
      loc_add_comment_list( a_dirent->d_name );
    }
  }
  closedir(fd);
}

char*
load_comment_if_exists(char *Name)
{
static char loc_comment_buffer[128];

  char        FileName[MAX_PATH];
  char        TmpFileName[MAX_PATH];
  FILE       *a_file;
  char       *SaveName;
  char       *Scan;

  loc_comment_buffer[0] = 0;

  strcpy(FileName, Name);
  SaveName = strrchr(FileName,'/');
  if (SaveName != (char *)0) SaveName++;
  else                       SaveName = FileName;

  Scan = strrchr(SaveName,'.');
  if (Scan) *Scan = '\0';

  if (!SaveName[0]) return 0;

  comment_list *scan_list = loc_head_comment;
  while (scan_list != 0) {
    if (! strcasecmp( SaveName, scan_list->name)) {
      snprintf(TmpFileName, MAX_PATH, "%s/txt/%s", ATARI.atari_home_dir, scan_list->filename);
      a_file = fopen(TmpFileName, "r");
      if (a_file) {
        char* a_scan = 0;
        loc_comment_buffer[0] = 0;
        if (fgets(loc_comment_buffer, 60, a_file) != 0) {
          a_scan = strchr(loc_comment_buffer, '\n');
          if (a_scan) *a_scan = '\0';
          /* For this #@$% of windows ! */
          a_scan = strchr(loc_comment_buffer,'\r');
          if (a_scan) *a_scan = '\0';
          fclose(a_file);
          return loc_comment_buffer;
        }
        fclose(a_file);
        return 0;
      }
    }
    scan_list = scan_list->next;
  }
  return 0;
}

void
atari_kbd_load(void)
{
  char        TmpFileName[MAX_PATH + 1];
  struct stat aStat;

  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", ATARI.atari_home_dir, ATARI.atari_save_name );
  if (! stat(TmpFileName, &aStat)) {
    psp_kbd_load_mapping(TmpFileName);
  }
}

int
atari_kbd_save(void)
{
  char TmpFileName[MAX_PATH + 1];
  snprintf(TmpFileName, MAX_PATH, "%s/kbd/%s.kbd", ATARI.atari_home_dir, ATARI.atari_save_name );
  return( psp_kbd_save_mapping(TmpFileName) );
}

void
atari_joy_load(void)
{
  char        TmpFileName[MAX_PATH + 1];
  struct stat aStat;

  snprintf(TmpFileName, MAX_PATH, "%s/joy/%s.joy", ATARI.atari_home_dir, ATARI.atari_save_name );
  if (! stat(TmpFileName, &aStat)) {
    psp_joy_load_settings(TmpFileName);
  }
}

int
atari_joy_save(void)
{
  char TmpFileName[MAX_PATH + 1];
  snprintf(TmpFileName, MAX_PATH, "%s/joy/%s.joy", ATARI.atari_home_dir, ATARI.atari_save_name );
  return( psp_joy_save_settings(TmpFileName) );
}

void
myPowerSetClockFrequency(int cpu_clock)
{
  if (ATARI.atari_current_clock != cpu_clock) {
    gp2xPowerSetClockFrequency(cpu_clock);
    ATARI.atari_current_clock = cpu_clock;
  }
}

void
atari_default_settings()
{
  //LUDO:
  ATARI.atari_snd_enable    = 1;
  ATARI.atari_render_mode   = ATARI_RENDER_NORMAL;
  ATARI.atari_vsync         = 0;
  ATARI.danzeff_trans       = 1;
  ATARI.atari_speed_limiter = 60;
  ATARI.psp_cpu_clock       = GP2X_DEF_EMU_CLOCK;
  ATARI.psp_screenshot_id   = 0;
  ATARI.atari_view_fps      = 0;

  myPowerSetClockFrequency(ATARI.psp_cpu_clock);
}

static int
loc_atari_save_settings(char *chFileName)
{
  FILE* FileDesc;
  int   error = 0;

  FileDesc = fopen(chFileName, "w");
  if (FileDesc != (FILE *)0 ) {

    fprintf(FileDesc, "psp_cpu_clock=%d\n"      , ATARI.psp_cpu_clock);
    fprintf(FileDesc, "danzeff_trans=%d\n"      , ATARI.danzeff_trans);
    fprintf(FileDesc, "psp_skip_max_frame=%d\n" , ATARI.psp_skip_max_frame);
    fprintf(FileDesc, "atari_view_fps=%d\n"     , ATARI.atari_view_fps);
    fprintf(FileDesc, "atari_snd_enable=%d\n"   , ATARI.atari_snd_enable);
    fprintf(FileDesc, "atari_render_mode=%d\n"  , ATARI.atari_render_mode);
    fprintf(FileDesc, "atari_vsync=%d\n"        , ATARI.atari_vsync);
    fprintf(FileDesc, "atari_speed_limiter=%d\n", ATARI.atari_speed_limiter);

    fclose(FileDesc);

  } else {
    error = 1;
  }

  return error;
}

int
atari_save_settings(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", ATARI.atari_home_dir, ATARI.atari_save_name);
  error = loc_atari_save_settings(FileName);

  return error;
}

static int
loc_atari_load_settings(char *chFileName)
{
  char  Buffer[512];
  char *Scan;
  unsigned int Value;
  FILE* FileDesc;

  FileDesc = fopen(chFileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    Scan = strchr(Buffer,'=');
    if (! Scan) continue;

    *Scan = '\0';
    Value = atoi(Scan+1);

    if (!strcasecmp(Buffer,"psp_cpu_clock"))      ATARI.psp_cpu_clock = Value;
    else
    if (!strcasecmp(Buffer,"danzeff_trans"))      ATARI.danzeff_trans = Value;
    else
    if (!strcasecmp(Buffer,"atari_view_fps"))  ATARI.atari_view_fps = Value;
    else
    if (!strcasecmp(Buffer,"psp_skip_max_frame")) ATARI.psp_skip_max_frame = Value;
    else
    if (!strcasecmp(Buffer,"atari_snd_enable"))   ATARI.atari_snd_enable = Value;
    else
    if (!strcasecmp(Buffer,"atari_render_mode"))  ATARI.atari_render_mode = Value;
    else
    if (!strcasecmp(Buffer,"atari_vsync"))  ATARI.atari_vsync = Value;
    else
    if (!strcasecmp(Buffer,"atari_speed_limiter"))  ATARI.atari_speed_limiter = Value;
  }

  fclose(FileDesc);

  myPowerSetClockFrequency(ATARI.psp_cpu_clock);

  return 0;
}

int
atari_load_settings()
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/set/%s.set", ATARI.atari_home_dir, ATARI.atari_save_name);
  error = loc_atari_load_settings(FileName);

  return error;
}

int
atari_load_file_settings(char *FileName)
{
  return loc_atari_load_settings(FileName);
}


void
emulator_reset(void)
{
  psp_atari_reset();
}

int
atari_load_rom(char *FileName, int zip_format)
{
  char  SaveName[MAX_PATH+1];
  char*  ExtractName;
  char*  scan;
  int    error;
  size_t unzipped_size;

  error = 1;

  if (zip_format) {

    ExtractName = find_possible_filename_in_zip( FileName, "a78.bin");
    if (ExtractName) {
      strncpy(SaveName, FileName, MAX_PATH);
      scan = strrchr(SaveName,'.');
      if (scan) *scan = '\0';
      atari_update_save_name(SaveName);
      const char* rom_buffer = extract_file_in_memory ( FileName, ExtractName, &unzipped_size);
      if (rom_buffer) {
        error = loc_load_rom_buffer( rom_buffer, unzipped_size );
      }
    }

  } else {
    strncpy(SaveName,FileName,MAX_PATH);
    scan = strrchr(SaveName,'.');
    if (scan) *scan = '\0';
    atari_update_save_name(SaveName);
    error = loc_load_rom(FileName);
  }

  if (! error ) {
    emulator_reset();
    atari_kbd_load();
    atari_joy_load();
    atari_load_cheat();
    atari_load_settings();
  }

  return error;
}

static int
loc_atari_save_cheat(char *chFileName)
{
  FILE* FileDesc;
  int   cheat_num;
  int   error = 0;

  FileDesc = fopen(chFileName, "w");
  if (FileDesc != (FILE *)0 ) {

    for (cheat_num = 0; cheat_num < ATARI_MAX_CHEAT; cheat_num++) {
      Atari_cheat_t* a_cheat = &ATARI.atari_cheat[cheat_num];
      if (a_cheat->type != ATARI_CHEAT_NONE) {
        fprintf(FileDesc, "%d,%x,%x,%s\n", 
                a_cheat->type, a_cheat->addr, a_cheat->value, a_cheat->comment);
      }
    }
    fclose(FileDesc);

  } else {
    error = 1;
  }

  return error;
}

int
atari_save_cheat(void)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", ATARI.atari_home_dir, ATARI.atari_save_name);
  error = loc_atari_save_cheat(FileName);

  return error;
}

static int
loc_atari_load_cheat(char *chFileName)
{
  char  Buffer[512];
  char *Scan;
  char *Field;
  unsigned int  cheat_addr;
  unsigned int  cheat_value;
  unsigned int  cheat_type;
  char         *cheat_comment;
  int           cheat_num;
  FILE* FileDesc;

  memset(ATARI.atari_cheat, 0, sizeof(ATARI.atari_cheat));
  cheat_num = 0;

  FileDesc = fopen(chFileName, "r");
  if (FileDesc == (FILE *)0 ) return 0;

  while (fgets(Buffer,512, FileDesc) != (char *)0) {

    Scan = strchr(Buffer,'\n');
    if (Scan) *Scan = '\0';
    /* For this #@$% of windows ! */
    Scan = strchr(Buffer,'\r');
    if (Scan) *Scan = '\0';
    if (Buffer[0] == '#') continue;

    /* %d, %x, %x, %s */
    Field = Buffer;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%d", &cheat_type) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_addr) != 1) continue;
    Field = Scan + 1;
    Scan = strchr(Field, ',');
    if (! Scan) continue;
    *Scan = 0;
    if (sscanf(Field, "%x", &cheat_value) != 1) continue;
    Field = Scan + 1;
    cheat_comment = Field;

    if (cheat_type <= ATARI_CHEAT_NONE) continue;

    Atari_cheat_t* a_cheat = &ATARI.atari_cheat[cheat_num];

    a_cheat->type  = cheat_type;
    a_cheat->addr  = cheat_addr;
    a_cheat->value = cheat_value;
    strncpy(a_cheat->comment, cheat_comment, sizeof(a_cheat->comment));
    a_cheat->comment[sizeof(a_cheat->comment)-1] = 0;

    if (++cheat_num >= ATARI_MAX_CHEAT) break;
  }
  fclose(FileDesc);

  return 0;
}

int
atari_load_cheat()
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  snprintf(FileName, MAX_PATH, "%s/cht/%s.cht", ATARI.atari_home_dir, ATARI.atari_save_name);
  error = loc_atari_load_cheat(FileName);

  return error;
}

int
atari_load_file_cheat(char *FileName)
{
  return loc_atari_load_cheat(FileName);
}

void
atari_apply_cheats()
{
  u8 *a_ram = get_memory_ram();

  int cheat_num;
  for (cheat_num = 0; cheat_num < ATARI_MAX_CHEAT; cheat_num++) {
    Atari_cheat_t* a_cheat = &ATARI.atari_cheat[cheat_num];
    if (a_cheat->type == ATARI_CHEAT_ENABLE) {
      a_ram[a_cheat->addr % ATARI_RAM_SIZE] = a_cheat->value;
    }
  }
}


static int
loc_load_state(char *filename)
{
  int error;
  error = psp_atari_load_state(filename);
  return error;
}

int
atari_load_state(char *FileName)
{
  char  SaveName[MAX_PATH+1];
  int   error;
  char  *scan;

  error = 1;

  strncpy(SaveName,FileName,MAX_PATH);
  scan = strrchr(SaveName,'.');
  if (scan) *scan = '\0';
  atari_update_save_name(SaveName);
  error = loc_load_state(FileName);

  if (! error ) {

    atari_kbd_load();
    atari_joy_load();
    atari_load_cheat();
    atari_load_settings();
  }

  return error;
}

static int
loc_atari_save_state(char *filename)
{
  int error;
  error = psp_atari_save_state(filename);
  return error;
}

int
atari_snapshot_save_slot(int save_id)
{
  char      FileName[MAX_PATH+1];
  struct stat aStat;
  int       error;

  error = 1;

  if (save_id < ATARI_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.sta", ATARI.atari_home_dir, ATARI.atari_save_name, save_id);
    error = loc_atari_save_state(FileName);
    if (! error) {
      if (! stat(FileName, &aStat)) {
        ATARI.atari_save_state[save_id].used  = 1;
        ATARI.atari_save_state[save_id].thumb = 0;
        ATARI.atari_save_state[save_id].date  = aStat.st_mtime;
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", ATARI.atari_home_dir, ATARI.atari_save_name, save_id);
        if (psp_sdl_save_thumb_png(ATARI.atari_save_state[save_id].surface, FileName)) {
          ATARI.atari_save_state[save_id].thumb = 1;
        }
      }
    }
  }

  return error;
}

int
atari_snapshot_load_slot(int load_id)
{
  char  FileName[MAX_PATH+1];
  int   error;

  error = 1;

  if (load_id < ATARI_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.sta", ATARI.atari_home_dir, ATARI.atari_save_name, load_id);
    error = loc_load_state(FileName);
  }
  return error;
}

int
atari_snapshot_del_slot(int save_id)
{
  char  FileName[MAX_PATH+1];
  struct stat aStat;
  int   error;

  error = 1;

  if (save_id < ATARI_MAX_SAVE_STATE) {
    snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.sta", ATARI.atari_home_dir, ATARI.atari_save_name, save_id);
    error = remove(FileName);
    if (! error) {
      ATARI.atari_save_state[save_id].used  = 0;
      ATARI.atari_save_state[save_id].thumb = 0;
      memset(&ATARI.atari_save_state[save_id].date, 0, sizeof(time_t));

      /* We keep always thumbnail with id 0, to have something to display in the file requester */ 
      if (save_id != 0) {
        snprintf(FileName, MAX_PATH, "%s/save/sav_%s_%d.png", ATARI.atari_home_dir, ATARI.atari_save_name, save_id);
        if (! stat(FileName, &aStat)) {
          remove(FileName);
        }
      }
    }
  }

  return error;
}

void
atari_treat_command_key(int atari_idx)
{
  int new_render;

  switch (atari_idx) 
  {
    case ATARIC_FPS: ATARI.atari_view_fps = ! ATARI.atari_view_fps;
    break;
    case ATARIC_JOY: ATARI.psp_reverse_analog = ! ATARI.psp_reverse_analog;
    break;
    case ATARIC_RENDER: 
      psp_sdl_black_screen();
      new_render = ATARI.atari_render_mode + 1;
      if (new_render > ATARI_LAST_RENDER) new_render = 0;
      ATARI.atari_render_mode = new_render;
    break;
    case ATARIC_LOAD: psp_main_menu_load_current();
    break;
    case ATARIC_SAVE: psp_main_menu_save_current(); 
    break;
    case ATARIC_RESET: 
       psp_sdl_black_screen();
       emulator_reset(); 
    break;
    case ATARIC_AUTOFIRE: 
       kbd_change_auto_fire(! ATARI.atari_auto_fire);
    break;
    case ATARIC_DECFIRE: 
      if (ATARI.atari_auto_fire_period > 0) ATARI.atari_auto_fire_period--;
    break;
    case ATARIC_INCFIRE: 
      if (ATARI.atari_auto_fire_period < 19) ATARI.atari_auto_fire_period++;
    break;
    case ATARIC_SCREEN: psp_screenshot_mode = 10;
    break;
  }
}

void
psp_global_initialize()
{
  memset(&ATARI, 0, sizeof(ATARI_t));
  strcpy(ATARI.atari_home_dir,".");
  atari_default_settings();
  psp_joy_default_settings();
  psp_kbd_default_settings();

  psp_sdl_init();

  atari_update_save_name("");
  atari_load_settings();
  atari_kbd_load();
  atari_joy_load();
  atari_load_cheat();

  myPowerSetClockFrequency(ATARI.psp_cpu_clock);
}

int 
SDL_main(int argc, char **argv)
{
  psp_global_initialize();

  psp_atari_main_loop();

  psp_sdl_exit(0);

  return 0;
}

