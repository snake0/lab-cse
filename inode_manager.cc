#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk() : blocks() {
    bzero(blocks, sizeof(blocks));
}

void
disk::read_block(uint bnum, char *buf) {
    /*
     *your code goes here.
     *if bnum is smaller than 0 or larger than BLOCK_NUM
     *or buf is null, just return.
     *put the content of target block into buf.
     *hint: use memcpy
    */
    if (!buf || bnum < 0 || bnum >= BLOCK_NUM)
        return;
    memcpy(buf, blocks[bnum], BLOCK_SIZE);
}

void
disk::write_block(uint bnum, const char *buf) {
    /*
     *your code goes here.
     *hint: just like read_block
    */
    if (!buf || bnum < 0 || bnum >= BLOCK_NUM)
        return;
    memcpy(blocks[bnum], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
uint
block_manager::alloc_block() {
    /*
     * your code goes here.
     * note: you should mark the corresponding bit in block bitmap when alloc.
     * you need to think about which block you can start to be allocated.
     */

    char buf[BLOCK_SIZE], mask;
    uint bnum;

    for (bnum = 0; bnum < BLOCK_NUM; bnum += BPB) {
        read_block(BBLOCK(bnum), buf);
        for (uint off = 0; off < BPB; ++off) {
            mask = (char) (1 << (off % 8));
            if ((buf[off / 8] & mask) == 0) {
                buf[off / 8] |= mask;
                write_block(BBLOCK(bnum), buf);
                return bnum + off;
            }
        }
    }
    return 0;
}

void
block_manager::free_block(uint bnum) {
    /*
     * your code goes here.
     * note: you should unmark the corresponding bit in the block bitmap when free.
     */

    if (bnum <= BBLOCK(BLOCK_NUM))
        return;

    char buf[BLOCK_SIZE];
    read_block(BBLOCK(bnum), buf);

    // set free bit
    uint offset = bnum % BPB;
    buf[offset / 8] &= ~(1 << (offset % 8));
    write_block(BBLOCK(bnum), buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager() : sb() {
    d = new disk();
    sb.size = BLOCK_SIZE * BLOCK_NUM;
    sb.nblocks = BLOCK_NUM;
    sb.ninodes = INODE_NUM;
    for (uint bnum = 0; bnum < BLOCK_NUM / BPB + INODE_NUM / IPB + 2; ++bnum)
        alloc_block();
}

void
block_manager::read_block(uint bnum, char *buf) {
    d->read_block(bnum, buf);
}

void
block_manager::write_block(uint bnum, const char *buf) {
    d->write_block(bnum, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager() {
    bm = new block_manager();
    uint root_dir = alloc_inode(extent_protocol::T_DIR);
    if (root_dir != 1) {
        printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
        exit(0);
    }
}

/* Create a new file.
 * Return its inum. */
uint
inode_manager::alloc_inode(uint type) {
    /*
     * your code goes here.
     * note: the normal inode block should begin from the 2nd inode block.
     * the 1st is used for root_dir, see inode_manager::inode_manager().
     * if you get some heap memory, do not forget to free it.
     */

    char buf[BLOCK_SIZE];

    // find a free block
    uint inum, off;
    for (inum = 1; inum < bm->sb.ninodes; ++inum) {
        off = (inum - 1) % IPB;
        if (!off)
            bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
        inode_t *inode = (inode_t *) buf + off;
        if (!inode->type) {
            inode->type = (short) type;
            inode->ctime = (uint) std::time(0);
            bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
            return inum;
        }
    }
    return 1;
}

void
inode_manager::free_inode(uint inum) {
    /*
     * your code goes here.
     * note: you need to check if the inode is already a freed one;
     * if not, clear it, and remember to write back to disk.
     * do not forget to free memory if necessary.
     */

    inode_t *inode = get_inode(inum);

    if (!inode->type)
        return;
    else {
        memset(inode, 0, sizeof(inode_t));
        put_inode(inum, inode);
        free(inode);
    }
}


/* Return an inode structure by inum, 0 otherwise.
 * Caller should release the memory. */
struct inode *
inode_manager::get_inode(uint inum) {
    struct inode *inode, *ino_disk;
    char buf[BLOCK_SIZE];

    printf("\tim: get_inode %d\n", inum);

    if (inum < 0 || inum >= INODE_NUM) {
        printf("\tim: inum out of range\n");
        return 0;
    }

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    // printf("%s:%d\n", __FILE__, __LINE__);

    ino_disk = (struct inode *) buf + inum % IPB;
    if (ino_disk->type == 0) {
        printf("\tim: inode not exist\n");
        return 0;
    }

    inode = (struct inode *) malloc(sizeof(struct inode));
    *inode = *ino_disk;

    return inode;
}

void
inode_manager::put_inode(uint inum, struct inode *inode) {
    char buf[BLOCK_SIZE];
    struct inode *ino_disk;

    printf("\tim: put_inode %d\n", inum);
    if (inode == 0)
        return;

    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode *) buf + inum % IPB;
    *ino_disk = *inode;
    bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a, b) (a < b ? a : b)

/* Get all the data of a file by inum.
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint inum, char **buf_out, int *size) {
    /*
     * your code goes here.
     * note: read blocks related to inode number inum,
     * and copy them to buf_out
     */

    inode_t *inode = get_inode(inum);

    if (!inode)
        return;

    uint fsize = inode->size, bnum, rsize = 0, toread = 0;
    char *res = (char *) malloc(fsize), buf[BLOCK_SIZE];

    for (bnum = 0; rsize < fsize && bnum < NDIRECT; ++bnum) {
        toread = MIN(BLOCK_SIZE, fsize - rsize);
        if (rsize + BLOCK_SIZE < fsize)
            bm->read_block(inode->blocks[bnum], res + rsize);
        else {
            bm->read_block(inode->blocks[bnum], buf);
            memcpy(res + rsize, buf, (uint) fsize - rsize);
        }
        rsize += toread;
    }

    if (rsize < fsize) {
        uint indnum[NINDIRECT];
        bm->read_block(inode->blocks[NDIRECT], (char *) indnum);

        for (bnum = 0; rsize < fsize && bnum < NINDIRECT; ++bnum) {
            toread = MIN(BLOCK_SIZE, fsize - rsize);
            if (rsize + BLOCK_SIZE < fsize)
                bm->read_block(indnum[bnum], res + rsize);
            else {
                bm->read_block(indnum[bnum], buf);
                memcpy(res + rsize, buf, (uint) fsize - rsize);
            }
            rsize += toread;
        }
    }
    *size = (int) fsize;
    *buf_out = res;
    uint now = (uint) std::time(0);
    inode->atime = now;
    inode->ctime = now;
    put_inode(inum, inode);
    free(inode);
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint inum, const char *buf, int size) {
    /*
     * your code goes here.
     * note: write buf to blocks of inode inum.
     * you need to consider the situation when the size of buf
     * is larger or smaller than the size of original inode.
     * you should free some blocks if necessary.
     */

    char block[BLOCK_SIZE], indnum[BLOCK_SIZE];
    inode_t *inode = get_inode(inum);
    uint osize = BLOCKS(inode->size), nsize = BLOCKS(size);

    // free all blocks
    for (uint bnum = 0; bnum < MIN(osize, NDIRECT); ++bnum)
        bm->free_block(inode->blocks[bnum]);
    if (osize > NDIRECT) {
        uint ind = osize - NDIRECT;
        bm->read_block(inode->blocks[NDIRECT], indnum);
        for (uint bnum = 0; bnum < ind; ++bnum)
            bm->free_block(*((uint *) indnum + bnum));
        bm->free_block(inode->blocks[NDIRECT]);
    }

    // alloc new blocks
    for (uint bnum = 0; bnum < MIN(nsize, NDIRECT); ++bnum)
        inode->blocks[bnum] = bm->alloc_block();
    if (nsize > NDIRECT) {
        inode->blocks[NDIRECT] = bm->alloc_block();
        bzero(indnum, BLOCK_SIZE);
        uint ind = nsize - NDIRECT;
        for (uint bnum = 0; bnum < ind; ++bnum)
            *((uint *) indnum + bnum) = bm->alloc_block();
        bm->write_block(inode->blocks[NDIRECT], indnum);
    }

    // write blocks
    int rsize = 0, towrite;
    uint bnum;
    for (bnum = 0; rsize < size && bnum < NDIRECT; ++bnum) {
        towrite = MIN(BLOCK_SIZE, size - rsize);
        if (size - rsize > BLOCK_SIZE)
            bm->write_block(inode->blocks[bnum], buf + rsize);
        else {
            memcpy(block, buf + rsize, (uint) size - rsize);
            bm->write_block(inode->blocks[bnum], block);
        }
        rsize += towrite;
    }

    if (rsize < size) {
        bm->read_block(inode->blocks[NDIRECT], indnum);
        for (bnum = 0; rsize < size && bnum < NINDIRECT; ++bnum) {
            uint ind = *((uint *) indnum + bnum);
            towrite = MIN(BLOCK_SIZE, size - rsize);
            if (size - rsize > BLOCK_SIZE)
                bm->write_block(ind, buf + rsize);
            else {
                memcpy(block, buf + rsize, (uint) size - rsize);
                bm->write_block(ind, block);
            }
            rsize += towrite;
        }
    }
    time_t now = std::time(0);
    inode->size = (uint) size;
    inode->mtime = (uint) now;
    inode->ctime = (uint) now;
    put_inode(inum, inode);
    free(inode);
}

void
inode_manager::getattr(uint inum, extent_protocol::attr &a) {
    /*
     * your code goes here.
     * note: get the attributes of inode inum.
     * you can refer to "struct attr" in extent_protocol.h
     */

    inode_t *inode = get_inode(inum);
    if (inode) {
        a.type = (uint) inode->type;
        a.atime = inode->atime;
        a.mtime = inode->mtime;
        a.ctime = inode->ctime;
        a.size = inode->size;
        free(inode);
    }
}

void
inode_manager::remove_file(uint inum) {
    /*
     * your code goes here
     * note: you need to consider about both the data block and inode of the file
     * do not forget to free memory if necessary.
     */

    inode_t *inode = get_inode(inum);

    if (!inode)
        return;
    uint bnum = BLOCKS(inode->size);
    if (bnum > NDIRECT) {
        uint indnum[NINDIRECT];
        bm->read_block(inode->blocks[NDIRECT], (char *) indnum);
        for (uint i = 0; i < bnum - NDIRECT; ++i)
            bm->free_block(indnum[i]);
        bm->free_block(inode->blocks[NDIRECT]);
    }

    for (uint i = 0; i < MIN(bnum, NDIRECT); ++i)
        bm->free_block(inode->blocks[i]);

    free_inode(inum);
    free(inode);
}

void
inode_manager::append_block(uint32_t inum, blockid_t &bid) {
    /*
     * your code goes here.
     */

}

void
inode_manager::get_block_ids(uint32_t inum, std::list<blockid_t> &block_ids) {
    /*
     * your code goes here.
     */

}

void
inode_manager::read_block(blockid_t id, char buf[BLOCK_SIZE]) {
    /*
     * your code goes here.
     */

}

void
inode_manager::write_block(blockid_t id, const char buf[BLOCK_SIZE]) {
    /*
     * your code goes here.
     */

}

void
inode_manager::complete(uint32_t inum, uint32_t size) {
    /*
     * your code goes here.
     */

}
