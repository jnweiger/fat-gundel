/*
 * fat-gundel.c - UNDELete Graphics from a FAT filesystem.
 * The name was inspired by the german brand 'gundel-putz':
 *
 * (C) 2006,2012 jw@suse.de, distribute under GPL-2.0 or ask.
 *
 * 2006-12-07, jw, V0.1  -- reads a fat superblock.
 *                          searches jpeg headers.
 * 2012-03-21, jw, V0.3  -- survive bad superblocks. 
 *                          Using ENV variables to configure.
 *
 * Wow. A medium that was emtpy before written has all the blocks in sequence 
 * So we just jump from jpeg magic to jpeg magic, and dump what is in between
 * as an image. That was easy.
 *
 * See also http://www.clarity.net/~adam/jpg-recover/jpg-recover
 * - it has a larger set of explicit jpeg start markers.
 * - it ignores sector boundaries.
 * - it detects end markers "\xff\xd9" - may yield truncated images.
 * See also git clone http://git.cgsecurity.org/testdisk.git
 * - it recognises many file types, and does sanity checks.
 * - PhotoRec is a signature based file recovery utility. It handles more than
 *   200 file formats including JPG, MSOffice, OpenOffice documents.
 *
 * 
 * TODO:
 * - add support for gif, tiff, and other graphics formats.
 * - add support for fragmented media: play puzzle with a 
 *   jpeg decoder, and output what has no errors.
 *
 * GIF support details:
 * start with GIF89a or GIF87a, 
 * next 16bits are width, lsb-first, next 16bits are height.
 * a follows \x2c somewhere, end in \x00\x3b
 * - should do some sanity checks with width*height to avoid truncated images.
 *
 */

#define VERSION "0.3"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>		// strcmp()

#if 0
#define LIMIT 10		// for faster debugging, stop after a few images.
#endif

static struct bootsector
{
  int sector_size;		// 512, 1024, 2048, 4096
  int sectors_total;		// total number of sectors
  int cluster_size;		// number of sectors per cluster (unused)
  char oem_name[10];		// 0-terminated.
  char volume_label[12];	// 0-terminated. fat12/16 only
  char fs_type[10];		// 0-terminated. fat12/16 only
  char fat_type;		// fat12, fat16, fat32
  int serial_num;		// fat12, 16 only.
} bs;

static int read_sector(int fd, unsigned char *buf, int sector_nr)
{
  if (lseek(fd, bs.sector_size * sector_nr, SEEK_SET) < 0)
    {
       fprintf(stderr, "seek(%d) ", bs.sector_size * sector_nr);
       perror("failed"); exit(0);
    }

  int n;
  if ((n = read(fd, buf, bs.sector_size)) < bs.sector_size)
    {
       fprintf(stderr, "read(%d, %d) = %d", bs.sector_size * sector_nr, bs.sector_size, n);
       perror("failed"); exit(0);
    }
  return n;
}

static int read_bootsector(int fd, unsigned char *buf)
{
  int i;
  int pause_secs = 0;
  bs.sector_size = 512;
  read_sector(fd, buf, 0);
  if (buf[510] != 0x55 && buf[511] != 0xaa)
    {
      fprintf(stderr, "FAT signature 55aa not found.\n- Make sure you specify the entire device.\n");
      if (!getenv("FAT_NO_SIG"))
        {
	  fprintf(stderr, "- Use env FAT_NO_SIG=1 to ignore this.\n");
          exit(0);
	}
      else
        {
	  fprintf(stderr, "... Continuing at your own risk\n");
	  pause_secs = 5;
	}
    }

  bs.sector_size   = buf[11] + (buf[12] << 8);
  bs.sectors_total = buf[19] + (buf[20] << 8);
  bs.cluster_size  = buf[13];
  for (i = 3; i <= 10; i++) bs.oem_name[i-3] = (buf[i] >= ' ' && buf[i] < 128) ? buf[i] : '#';
  bs.oem_name[i-3] = '\0';

  int ss;
  char *e;
  switch (bs.sector_size)
    {
    case  512: 
    case 1024: 
    case 2048: 
    case 4096:
      break;
    default:
      fprintf(stderr, "invalid sector size %d\n", bs.sector_size);
      e = getenv("FAT_SECTOR_SIZE");
      ss = atoi(e ? e : "512");
      fprintf(stderr, "Defaulting to %d,\n override with e.g. env FAT_SECTOR_SIZE=1024\n", ss);
      bs.sector_size = ss;
    }

  if ((buf[38] == 0x29) || (buf[38] == 0x28)) 
    bs.fat_type = 16;
  if (buf[66] == 0x29) 
    bs.fat_type = 32;
  else 
    {
      e = getenv("FAT_TYPE");
      ss = atoi(e ? e : "16");
      fprintf(stderr, "Defaulting to fat%d,\n override with e.g. env FAT_TYPE=32\n", ss);
      bs.fat_type = ss;
    }

  if ((bs.sectors_total == 0) && (bs.fat_type > 12))
    {
      bs.sectors_total = buf[32]|(buf[33]<<8)|(buf[34]<<16)|(buf[35]<<24);
    }

  if (bs.fat_type == 16)
    {
      bs.serial_num = buf[39]|(buf[40]<<8)|(buf[41]<<16)|(buf[42]<<24);

      for (i = 43; i <= 53; i++) bs.volume_label[i-43] = (buf[i] >= ' ' && buf[i] < 128) ? buf[i] : '#';
      bs.volume_label[i-43] = '\0';

      for (i = 54; i <= 61; i++) bs.fs_type[i-54] = (buf[i] >= ' ' && buf[i] < 128) ? buf[i] : '#';
      bs.fs_type[i-54] = '\0';
    }
  else if (bs.fat_type == 32)
    {
      bs.serial_num = buf[67]|(buf[68]<<8)|(buf[69]<<16)|(buf[70]<<24);

      for (i = 71; i <= 81; i++) bs.volume_label[i-71] = (buf[i] >= ' ' && buf[i] < 128) ? buf[i] : '#';
      bs.volume_label[i-71] = '\0';

      for (i = 82; i <= 89; i++) bs.fs_type[i-82] = (buf[i] >= ' ' && buf[i] < 128) ? buf[i] : '#';
      bs.fs_type[i-82] = '\0';
    }

  if (bs.sectors_total == 0 || bs.sectors_total == 0xffff) 
    {
      fprintf(stderr, "sectors_total=%d appears invalid. Trying sizeof().\n", bs.sectors_total);
      bs.sectors_total = lseek(fd, 0, SEEK_END) / bs.sector_size;
      fprintf(stderr, " got sectors_total=%d, override with e.g. env FAT_SECTORS_TOTAL=31332352\n", bs.sectors_total);
    }
  if ((e = getenv("FAT_SECTORS_TOTAL")))
    {
      ss = atoi(e);
      bs.sectors_total = ss;
    }

  printf("fat%d: sector_size=%d, sectors_total=%d, sectors_per_cluster=%d, oem_name='%s'\n", 
    bs.fat_type, bs.sector_size, bs.sectors_total, bs.cluster_size, bs.oem_name);
  if (bs.fat_type > 12)
    printf("volume_label='%s', fs_type='%s', serial_num=0x%08x\n", 
      bs.volume_label, bs.fs_type, bs.serial_num);

  if (pause_secs) 
    {
      fprintf(stderr, "Waiting %d sec for your review -- press CTRL-C to abort.\n", pause_secs);
      sleep(pause_secs);
    }
  return bs.fat_type;
}

static int find_jpeg_header(int fd, unsigned char *buf, int sector)
{
  // 0x20200
  unsigned char t[5] = "jpeg";
  read_sector(fd, buf, sector);
  if ((buf[0] != 0xff) || (buf[1] != 0xd8)) return 0;	// no jpeg magic
  if ((buf[2] == 0xff) && ((buf[3] & 0xfe) == 0xe0))
    {
       int i;
       for (i = 6; i <= 9; i++)
         {
	   t[i-6] = buf[i];
	 }
    }
  t[4] = '\0';

  printf("ffd8 %s at 0x%x sector %d\n", t, bs.sector_size * sector, sector);
  return ((t[0] << 8)| t[1]);
}

struct image
{
  int id;		// sequence number of the image
  int type;		// 1=jpeg, 2=JFIF, 3=Exif
  int start;		// first sector_number;
};

struct image_list
{
  int max;		// allocated
  int cnt;		// used
  struct image *img;	// list
};

static struct image_list *find_images(int fd, unsigned char *buf)
{
  int i;
  static struct image_list l;
  l.max = 128;
  l.cnt = 0;
  l.img = malloc(l.max * sizeof(struct image));

  for (i = 1; i < bs.sectors_total; i++)
    {
      int t;
      if ((t = find_jpeg_header(fd, buf, i)))
        {
	  if (l.cnt >= l.max)
	    {
	      l.max *= 2;
	      l.img = realloc(l.img, l.max * sizeof(struct image));
	    }

	  l.img[l.cnt].id = l.cnt;
	  l.img[l.cnt].start = i;
	  l.img[l.cnt].type = t;
	  l.cnt++;
	}
      if ((i & 0x0f) == 0) fprintf(stderr, " %d     \t%d      \r", bs.sectors_total-i, l.cnt);
#ifdef LIMIT
      if (l.cnt > LIMIT) i = bs.sectors_total;
#endif
    }
  fprintf(stderr, "             \r%d candidates found.\n", l.cnt);
  return &l;
}

struct sector
{
  unsigned short img_nr;	// image number this sector belongs to. 0xffff if unused.
};

// returns an array of struct sectors.
static struct sector *sect_list(struct image_list *il)
{
  struct sector *s = malloc(bs.sectors_total * sizeof(struct sector));
  int i;

  for (i = 0; i < bs.sectors_total; i++)
    {
      s[i].img_nr = 0xffff;
    }
  for (i = 0; i < il->cnt; i++)
    {
      s[il->img[i].start].img_nr = i;
    }
  return s;
}

int main(int ac, char **av)
{
  int ifd;
  unsigned char buf[4096];
  int i;
  char *prefix = "./gundel_";

  if (ac != 2 && ac != 3)
    {
      fprintf(stderr, "fat-gundel %s Usage:\n\t%s block_dev [outputdir/prefix]\n\n\
outputdir and prefix defaults to '%s'.\n", VERSION, av[0], prefix);
    }

  if (ac == 3) prefix = av[2];

  if (!av[1] || !strcmp(av[1], "-h") || !strcmp(av[1], "--help"))
    {
      fprintf(stderr, "\nThe following environment variables help with a corrupt boot sector:\n");
      fprintf(stderr, " FAT_NO_SIG=1               ignore missing FAT signature\n");
      fprintf(stderr, " FAT_SECTOR_SIZE=512        overwrite the sector size\n");
      fprintf(stderr, " FAT_SECTORS_TOTAL=2000000  specify number of sectors.\n\n");
      exit(0);
    }

  if ((ifd = open(av[1], O_RDONLY)) == -1)
    {
      fprintf(stderr, "Cannot open %s ", av[1]);
      perror("for reading"); exit(1);
    }

  read_bootsector(ifd, buf);

  fprintf(stderr, "searching %s ...\n", av[1]);
  struct image_list *il = find_images(ifd, buf);
  struct sector *sl = sect_list(il);

  fprintf(stderr, "writing to %s ...\n", av[2]);
  for (i = 0; i < il->cnt; i++)
    {
      int n = 0;
      int sect = il->img[i].start;
      char oname[20];

      sprintf(oname, "%s%04d.jpg", prefix, i);
      int ofd = open(oname, O_WRONLY|O_CREAT, 0644);
      if (ofd < 0)
        {
	  fprintf(stderr, "Cannot write %s", oname);
	  perror(""); exit (0);
	}

      while ((sl[sect].img_nr == 0xffff) || (sl[sect].img_nr == i))
        {
          read_sector(ifd, buf, sect);
          if (write(ofd, buf, bs.sector_size) < bs.sector_size)
	    {
	      fprintf(stderr, "%s: ", oname);
	      perror("write fails"); exit (0);
	    }
	  sect++;
	  n++;
	}

      if (close(ofd))
        {
	  fprintf(stderr, "%s: ", oname);
	  perror("final write failed"); exit(0);
	}

      double kbyte = n*bs.sector_size/1024.0;
      char ss = 'k';
      if (kbyte > 1024.)
        {
	  ss = 'M';
	  kbyte = kbyte / 1024.0;
	}
      fprintf(stderr, "%s written. (%.1f%c)\n", oname, kbyte, ss);
      fprintf(stderr, " %d%% done\r", (i+1)*100/il->cnt);
#ifdef LIMIT
      if (i == (LIMIT-1))
        break;		// last image may get huge, if not all starts were seen.
#endif
    }

  close(ifd);
  exit(0);
}
