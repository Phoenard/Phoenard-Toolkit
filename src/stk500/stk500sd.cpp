#include "stk500sd.h"

stk500sd::stk500sd(stk500 *handler) {
    _handler = handler;
    reset();
}

void stk500sd::reset() {
    _volume.isInitialized = 0;
    for (int i = 0; i < SD_CACHE_CNT; i++) {
        _cache[i].reset();
    }
}

void stk500sd::init(bool forceInit) {
    /* Various conditions to check whether we are initialized already */
    if (!forceInit && (_volume.isInitialized == 1) && !_handler->isTimeout()) return;

    /* Reset the cache and volume state before initializing */
    reset();

    /* Initialize the SD card on the device */
    _volume = _handler->SD_init();

    /* Debug */
    //debugPrint();
}

CardVolume stk500sd::volume() {
    init();
    return _volume;
}

void stk500sd::debugPrint() {
    qDebug() << (_volume.isfat16 ? "FAT16" : "FAT32") << ":(BlocksPerClust=" << _volume.blocksPerCluster <<
                ",FATStartBlock=" << _volume.fatStartBlock << ",RootClust=" << _volume.rootCluster <<
                ",DataStartBlock=" << _volume.dataStartBlock << ")";
}

/* ============================ Cache handling =========================== */

char* stk500sd::cacheBlock(quint32 block, bool readBlock, bool markDirty, bool isFat) {
    /* Start by marking all caches off by 1 */
    for (int i = 0; i < SD_CACHE_CNT; i++) {
        if (_cache[i].usageCtr > 0) {
            _cache[i].usageCtr--;
        }
    }

    /*
     * See if data is still contained in the cache; if so return that
     * At the same time track which cache has the lowest use counter
     */
    int useMin = 0xFFFF;
    BlockCache *selected;
    for (int i = 0; i < SD_CACHE_CNT; i++) {
        // Is it the block requested? Return right away!
        if (_cache[i].block == block) {
            /* Found it! Mark dirty if needed and update usage, then return it */
            _cache[i].needsWriting |= markDirty;
            _cache[i].usageCtr += 2;
            return _cache[i].buffer;
        }

        // If lower usage counter than before, use this one instead
        if (_cache[i].usageCtr < useMin) {
            useMin = _cache[i].usageCtr;
            selected = &_cache[i];
        }
    }

    /* If old cache dirty, write it out first */
    if (selected->needsWriting) {
        writeOutCache(selected);
    }

    /* Update cache information */
    selected->block = block;
    selected->isFAT = isFat;
    selected->needsWriting = markDirty;
    selected->usageCtr = 1;

    /* Read into memory if specified */
    if (readBlock) {
        try {
            init();
            _handler->SD_readBlock(selected->block, selected->buffer, 512);
        } catch (ProtocolException&) {
            BlockCache cache_old = *selected;
            _handler->reset();
            init();
            *selected = cache_old;
            _handler->SD_readBlock(selected->block, selected->buffer, 512);
        }
    }
    return selected->buffer;
}

void stk500sd::writeOutCache(BlockCache *cache) {
    try {
         init();
        _handler->SD_writeBlock(cache->block, cache->buffer, 512, cache->isFAT);
    } catch (ProtocolException&) {
        BlockCache cache_old = *cache;
        _handler->reset();
         init();
         *cache = cache_old;
        _handler->SD_writeBlock(cache->block, cache->buffer, 512, cache->isFAT);
    }
    cache->reset();
}

void stk500sd::flushCache() {
    for (int i = 0; i < SD_CACHE_CNT; i++) {
        if (_cache[i].needsWriting) {
            writeOutCache(&_cache[i]);
        }
    }
}

void stk500sd::read(quint32 block, int blockOffset, char* dest, int length) {
    if (blockOffset < 0 || (length + blockOffset) > 512) {
        QString errorMsg = "Reading from outside the block, offset: ";
        errorMsg.append(QString::number(blockOffset));
        throw ProtocolException(errorMsg);
    }
    memcpy(dest, cacheBlock(block, true, false) + blockOffset, length);
}

void stk500sd::write(quint32 block, int blockOffset, char* src, int length) {
    if (blockOffset < 0 || (length + blockOffset) > 512) {
        QString errorMsg = "Writing to outside the block, offset: ";
        errorMsg.append(QString::number(blockOffset));
        throw ProtocolException(errorMsg);
    }
    if (length == 512) {
        memcpy(cacheBlock(block, false, true), src, 512);
    } else {
        memcpy(cacheBlock(block, true, true) + blockOffset, src, length);
    }
}

void stk500sd::wipeBlock(quint32 block, bool isFat) {
    memset(cacheBlock(block, false, true, isFat), 0, 512);
}

void stk500sd::wipeCluster(quint32 cluster) {
    init();
    quint32 block = getClusterBlock(cluster);
    for (int i = 0; i < _volume.blocksPerCluster; i++) {
        wipeBlock(block + i);
    }
}

DirectoryEntryPtr stk500sd::nextDirectory(DirectoryEntryPtr dir_ptr, int count, bool create) {
    bool isFat16Root = false;
    quint32 fat16LastRoot = 0;

    init();
    if (_volume.isfat16) {
        fat16LastRoot = _volume.rootCluster + _volume.rootSize - 1;
        isFat16Root = (dir_ptr.block >= _volume.rootCluster && dir_ptr.block < fat16LastRoot);
    }

    DirectoryEntryPtr next = dir_ptr;
    while (count--) {
        if (++next.index >= 16) {
            next.index = 0;

            // Is this entry in FAT16 root?
            if (isFat16Root) {
                // Invalid, out of bounds
                if (next.block == fat16LastRoot) {
                    return DirectoryEntryPtr(0, 0);
                }
                // Next
                next.block++;
            } else {
                // Find the cluster of the current block, and from that the start block
                quint32 currentCluster = getClusterFromBlock(next.block);
                quint32 startBlock = getClusterBlock(currentCluster);
                if (++next.block >= (startBlock + _volume.blocksPerCluster)) {
                    // Find next cluster
                    quint32 nextCluster = fatGet(currentCluster);
                    if (isEOC(nextCluster)) {
                        // Last entry, return invalid unless create is specified
                        if (create) {
                            nextCluster = findFreeCluster(currentCluster);
                            wipeCluster(nextCluster);
                            fatPut(currentCluster, nextCluster);
                            fatPut(nextCluster, CLUSTER_EOC);
                        } else {
                            // No more...
                            return DirectoryEntryPtr(0, 0);
                        }
                    }
                    // Set to first block of next cluster
                    next.block = getClusterBlock(nextCluster);
                }
            }
        }
    }
    return next;
}

DirectoryEntry stk500sd::readDirectory(DirectoryEntryPtr entryPtr) {
    int dir_sz = sizeof(DirectoryEntry);
    DirectoryEntry entry;
    read(entryPtr.block, entryPtr.index * dir_sz, (char*) &entry, dir_sz);
    return entry;
}

void stk500sd::writeDirectory(DirectoryEntryPtr entryPtr, DirectoryEntry entry) {
    int dir_sz = sizeof(DirectoryEntry);
    write(entryPtr.block, entryPtr.index * dir_sz, (char*) &entry, dir_sz);
}

void stk500sd::wipeDirectory(DirectoryEntryPtr entryPtr) {
    DirectoryEntry newEntry;
    memset(&newEntry, 0, sizeof(DirectoryEntry));
    writeDirectory(entryPtr, newEntry);
}

/* ============================== FAT Cluster handling ============================= */

bool stk500sd::isEOC(quint32 cluster) {
    return !cluster || cluster >= (_volume.isfat16 ? FAT16EOC_MIN : FAT32EOC_MIN);
}

quint32 stk500sd::getClusterBlock(quint32 cluster) {
    init();
    if (cluster < 2) {
        if (_volume.isfat16) {
            return _volume.rootCluster;
        } else {
            cluster = _volume.rootCluster;
        }
    }
    return _volume.dataStartBlock + (cluster - 2) * _volume.blocksPerCluster;
}

quint32 stk500sd::getClusterFromBlock(quint32 block) {
    init();
    if (block < _volume.dataStartBlock) {
        return 0;
    }
    return ((block - _volume.dataStartBlock) / _volume.blocksPerCluster) + 2;
}

quint32 stk500sd::fatGet(quint32 cluster) {
    init();
    quint32 block;
    if (_volume.isfat16) {
        block = _volume.fatStartBlock + (cluster >> 8);
    } else {
        block = _volume.fatStartBlock + (cluster >> 7);
    }
    unsigned char* fatBlockData = (unsigned char*) cacheBlock(block, true, false, true);
    quint32 next_cluster = 0;
    if (_volume.isfat16) {
        uint idx = (cluster & 0XFF) << 1;
        next_cluster |= (quint32) (fatBlockData[idx + 0] << 0);
        next_cluster |= (quint32) (fatBlockData[idx + 1] << 8);
    } else {
        uint idx = (cluster & 0X7F) << 2;
        next_cluster |= ((quint32) fatBlockData[idx + 0] << 0);
        next_cluster |= ((quint32) fatBlockData[idx + 1] << 8);
        next_cluster |= ((quint32) fatBlockData[idx + 2] << 16);
        next_cluster |= ((quint32) fatBlockData[idx + 3] << 24);
        next_cluster &= FAT32MASK;
    }
    return next_cluster;
}

void stk500sd::fatPut(quint32 cluster, quint32 clusterNext) {
    init();
    quint32 block;
    if (_volume.isfat16) {
        block = _volume.fatStartBlock + (cluster >> 8);
    } else {
        block = _volume.fatStartBlock + (cluster >> 7);
    }
    unsigned char* fatBlockData = (unsigned char*) cacheBlock(block, true, true, true);
    if (_volume.isfat16) {
        quint16 idx = (cluster & 0XFF) << 1;
        clusterNext &= 0xFFFF;
        fatBlockData[idx + 0] = (clusterNext >> 0) & 0xFF;
        fatBlockData[idx + 1] = (clusterNext >> 8) & 0xFF;
    } else {
        quint16 idx = (cluster & 0X7F) << 2;
        fatBlockData[idx + 0] = (clusterNext >> 0) & 0xFF;
        fatBlockData[idx + 1] = (clusterNext >> 8) & 0xFF;
        fatBlockData[idx + 2] = (clusterNext >> 16) & 0xFF;
        fatBlockData[idx + 3] = (clusterNext >> 24) & 0xFF;
    }
}

quint32 stk500sd::findFreeCluster(quint32 startCluster) {
    quint32 cluster = startCluster;
    quint32 lastCluster = volume().clusterLast;
    do {
        /* Reached (the other way) around the start? Fail! */
        if (++cluster == startCluster) {
            throw ProtocolException("No more space left on the Micro-SD volume");
        }

        /* Past end - start from beginning of FAT */
        if (cluster > lastCluster) {
            cluster = 2;
        }

        /* Found our cluster when the next cluster is cleared */
    } while (fatGet(cluster) != 0);
    return cluster;
}

void stk500sd::wipeClusterChain(quint32 startCluster) {
    quint32 cluster = startCluster;
    quint32 cluster_next;
    while (true) {
        cluster_next = fatGet(cluster);
        fatPut(cluster, 0);
        if (isEOC(cluster_next)) {
            break;
        } else {
            cluster = cluster_next;
        }
    }
}

/* =========================== Directory walker API =========================== */

DirectoryEntryPtr stk500sd::getRootPtr() {
    quint32 block;
    if (volume().isfat16) {
        block = volume().rootCluster;
    } else {
        block = getClusterBlock(volume().rootCluster);
    }
    return DirectoryEntryPtr(block, 0);
}

DirectoryEntryPtr stk500sd::getDirPtrFromCluster(quint32 cluster) {
    if (cluster) {
        return DirectoryEntryPtr(getClusterBlock(cluster), 0);
    } else {
        return getRootPtr();
    }
}
