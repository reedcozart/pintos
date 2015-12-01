#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
                 /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    block_sector_t block_list[126];  
  };

  struct i_inode_disk{
    block_sector_t block_list[128];
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

void inode_shrink(struct inode_disk *, off_t len);
void inode_grow(struct inode_disk *, off_t len);



/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length){
    int sector = pos / BLOCK_SECTOR_SIZE;
    if(sector < 124) 
      return inode->data.block_list[sector];
    if(sector < 252){
      //indirect block is being accessed
      index = calloc(1, sizeof *index);
      index = cache_read(inode, inode->data.block_list[124]);
      block_sector_t return_val = index->block_list[sector - 124];
      free(index);
      return return_val;
    }
    if (sector < 16636){
      //doubly indirect block (gross)
      index = calloc(1, sizeof *index);
      index = cache_read(inode, inode->data.block_list[125]);
      block_sector_t next_block = index->block_list[(sector - 252)/128];
      index = cache_read(inode, next);
      block_sector_t return_val = index->block_list[(sector-252)%128];
      free(index);
      return return_val;
    }
    return -1; //everything failed...
    //return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  }
  else{
    return -1;
  }
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}
// Removes and deallocs any blocks within the inode.
void shrink(struct inode_disk *disk_inode, off_t length) {
  size_t sectors = bytes_to_sectors(length);
  struct i_inode_disk *dbl_indirect = NULL;
  struct i_inode_disk *indirect = NULL;
  int i;
  int j;
  int count;
  
  // doubly indirect blocks
  if (sectors < 16636 && disk_inode->block_list[125] != -1) {
    dbl_indirect = disk_inode->block_list[125];
    for (i = 127; i >= 0; i--) {
      if (sectors < 16636 - 128 * (127 - i) && dbl_indirect->block_list[i] != -1) {
        indirect = dbl_indirect->block_list[i];
        for (j = 127; j >= 0; j--) {
          if (sectors < 16636 - 128 * (127 - i) - (127 - j) && indirect->block_list[j] != -1) {
            free_map_release(indirect->block_list[j], 1);
            indirect->block_list[j] = -1;
          }
        }
        if (sectors < 16636 - 128 * (127 - i) - 127) {
          free_map_release(indirect, 1);
          dbl_indirect->block_list[i] = -1;
        }
        else {
          lock_acquire(filesys_lock_list + dbl_indirect->block_list[i]);
          block_write(fs_device, dbl_indirect->block_list[i], indirect);
          lock_release(filesys_lock_list + dbl_indirect->block_list[i]);
        }
      }
    }
    if (sectors < 253) {
      free_map_release(dbl_indirect, 1);
      disk_inode->block_list[125] = -1;
    }
    else {
      lock_acquire(filesys_lock_list + disk_inode->block_list[125]);
      block_write(fs_device, disk_inode->block_list[125], dbl_indirect);
      lock_release(filesys_lock_list + disk_inode->block_list[125]);
    }
  }
  
  // indirect blocks
  if (sectors < 252 && disk_inode->block_list[124] != -1) {
    indirect = disk_inode->block_list[124];
    for (i = 127; i >= 0; i--) {
      if (sectors < 252 - (127 - i)) {
        free_map_release(indirect->block_list[i], 1);
        indirect->block_list[i] = -1;
      }
    }
    if (sectors < 125) {
      free_map_release(indirect, 1);
      disk_inode->block_list[124] = -1;
    }
    else {
      lock_acquire(filesys_lock_list + disk_inode->block_list[124]);
      block_write(fs_device, disk_inode->block_list[124], indirect);
      lock_release(filesys_lock_list + disk_inode->block_list[124]);
    }
  }
  
  // direct blocks
  for (i = 123; i >= 0; i--) {
    if (sectors < 124 - (123 - i) && disk_inode->block_list[i] != -1) {
      free_map_release(disk_inode->block_list[i], 1);
      disk_inode->block_list[i] = -1;
    }
  }
  
  disk_inode->length = length;
}


// Grows the file to a given length.
bool grow(struct inode_disk *disk_inode, off_t length) {
  size_t sectors = bytes_to_sectors(length);
  size_t cur_sectors = bytes_to_sectors(disk_inode->length);
  struct i_inode_disk *indirect = NULL;
  struct i_inode_disk *dbl_indirect = NULL;
  uint32_t i;
  uint32_t j;
  block_sector_t file_sector = 0;
  block_sector_t indirect_sector = 0;
  block_sector_t dbl_indirect_sector = 0;
  static char zeros[BLOCK_SECTOR_SIZE];
  
  size_t cur = bytes_to_sectors(disk_inode->length);
  int growth = sectors - cur;
  if (growth <= 0) {
    disk_inode->length = length;
    return true;
  }
  
  // direct blocks
  if (cur < 124) {
    for (i = cur; i < sectors && i < 124; i++) {
      if (free_map_allocate(1, &file_sector)) {
        lock_acquire(filesys_lock_list + file_sector);
        block_write(fs_device, file_sector, zeros);
        lock_release(filesys_lock_list + file_sector);
        disk_inode->block_list[i] = file_sector;
        growth--;
      }
      else {
        shrink(disk_inode, cur_sectors);
        return false;
      }
    }
    cur = 124;
  }
  if (growth <= 0) {
    disk_inode->length = length;
    return true;
  }
  
  // indirect blocks
  if (cur < 252) {
    if (disk_inode->block_list[124] == -1) {
      indirect = calloc(1, sizeof *indirect);
      if (indirect == NULL) {
        shrink(disk_inode, cur_sectors);
        return false;
      }
      if (free_map_allocate(1, &indirect_sector)) {
        for (i = 0; i < 128; i++) {
          indirect->block_list[i] = -1;
        }
      }
      else {
        shrink(disk_inode, cur_sectors);
        return false;
      }
    }
    else {
      block_read(fs_device, disk_inode->block_list[124], indirect);
    }
    for (i = cur; i < (sectors - 124) && i < 128; i++) {
      if (free_map_allocate(1, &file_sector)) {
        lock_acquire(filesys_lock_list + file_sector);
        block_write(fs_device, file_sector, zeros);
        lock_release(filesys_lock_list + file_sector);
        indirect->block_list[i] = file_sector;
        growth--;
      }
      else {
        shrink(disk_inode, cur_sectors);
        return false;
      }
    }
    if (disk_inode->block_list[124] == -1) {
      disk_inode->block_list[124] = indirect_sector;
      lock_acquire(filesys_lock_list + disk_inode->block_list[124]);
      block_write(fs_device, disk_inode->block_list[124], indirect);
      lock_release(filesys_lock_list + disk_inode->block_list[124]);
      free(indirect);
    }
    else {
      lock_acquire(filesys_lock_list + disk_inode->block_list[124]);
      block_write(fs_device, disk_inode->block_list[124], indirect);
      lock_release(filesys_lock_list + disk_inode->block_list[124]);
    }
    cur = 252;
  }
  if (growth <= 0) {
    disk_inode->length = length;
    return true;
  }
  
  // doubly indirect blocks
  if (cur < 16636) {
    if (disk_inode->block_list[125] == -1) {
      dbl_indirect = calloc(1, sizeof *dbl_indirect);
      if (dbl_indirect == NULL) {
        shrink(disk_inode, cur);
        return false;
      }
      if (free_map_allocate(1, &dbl_indirect_sector)) {
        for (i = 0; i < 128; i++) {
          dbl_indirect->block_list[i] = -1;
        }
      }
      else {
        shrink(disk_inode, cur);
        return false;
      }
    }
    else {
      block_read(fs_device, disk_inode->block_list[125], dbl_indirect);
    }
    for (i = (cur - 252) / 128; i < (sectors - 252) / 128 + 1 && i < 128; i++) {
      if (dbl_indirect->block_list[i] == -1) {
        indirect = calloc(1, sizeof *indirect);
        if (indirect == NULL) {
          shrink(disk_inode, cur_sectors);
          return false;
        }
        if (free_map_allocate(1, &indirect_sector)) {
          for (i = 0; i < 128; i++) {
            indirect->block_list[i] = -1;
          }
        }
        else {
          shrink(disk_inode, cur_sectors);
          return false;
        }
      }
      else {
        block_read(fs_device, dbl_indirect->block_list[i], indirect);
      }
      for (j = cur - 252 - (i * 128); j < sectors - 252 - (i * 128) + 1 && j < 128; j++) {
        if (free_map_allocate(1, &file_sector)) {
          lock_acquire(filesys_lock_list + file_sector);
          block_write(fs_device, file_sector, zeros);
          lock_release(filesys_lock_list + file_sector);
          indirect->block_list[i] = file_sector;
          growth--;
        }
        else {
          shrink(disk_inode, cur_sectors);
          return false;
        }
      }
      if (dbl_indirect->block_list[i] == -1) {
        dbl_indirect->block_list[i] = indirect_sector;
        lock_acquire(filesys_lock_list + dbl_indirect->block_list[i]);
        block_write(fs_device, dbl_indirect->block_list[i], indirect);
        lock_release(filesys_lock_list + dbl_indirect->block_list[i]);
        free(indirect);
      }
      else {
        lock_acquire(filesys_lock_list + dbl_indirect->block_list[i]);
        block_write(fs_device, dbl_indirect->block_list[i], indirect);
        lock_release(filesys_lock_list + dbl_indirect->block_list[i]);
      }
    }
    if (disk_inode->block_list[125] == -1) {
      disk_inode->block_list[125] = dbl_indirect_sector;
      lock_acquire(filesys_lock_list + disk_inode->block_list[125]);
      block_write(fs_device, disk_inode->block_list[125], dbl_indirect);
      lock_release(filesys_lock_list + disk_inode->block_list[125]);
      free(dbl_indirect);
    }
    else {
      lock_acquire(filesys_lock_list + disk_inode->block_list[125]);
      block_write(fs_device, disk_inode->block_list[125], dbl_indirect);
      lock_release(filesys_lock_list + disk_inode->block_list[125]);
    }
  }
  if (growth <= 0) {
    disk_inode->length = length;
    return true;
  }
  return false; // should never get here
}
/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;
  int i;
  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = 0;
      disk_inode->magic = INODE_MAGIC;
      for(i = 0; i<126; i++ ){
        disk_inode->block_list[i] = -1;
      }
      success = grow(disk_inode, length);
      if(success){
        lock_acquire(filesys_lock_list + sector);
        block_write(fs_device, sector, disk_inode);
        lock_release(filesys_lock_list + sector);
      }
      free(disk_inode);
      return success;
      /*
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
    }
  return success;*/
  }
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
          shrink(&(inode->data), 0);
        }

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_idx == -1 )
        return bytes_read;

      bounce = cache_read(inode, sector_idx);
      if(bounce == NULL)
        break;
      memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);

      /*if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. 
          //TODO Call buffer-cache here instead of block read
          cache_read(sector_idx, buffer + bytes_read);
          //block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          //TODO Call buffer-cache here instead of block read
          cache_read(sector_idx, buffer + bytes_read);
          //block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        } */
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}
