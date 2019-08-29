// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include <coins.h>
#include <dbwrapper.h>
#include <chain.h>
#include <veil/ringct/rctindex.h>
#include <primitives/block.h>
#include <libzerocoin/Coin.h>
#include <libzerocoin/CoinSpend.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

class CBlockIndex;
class CCoinsViewDBCursor;
class uint256;

const char DB_RCTOUTPUT = 'A';
const char DB_RCTOUTPUT_LINK = 'L';
const char DB_RCTKEYIMAGE = 'K';

//! No need to periodic flush if at least this much space still available.
static constexpr int MAX_BLOCK_COINSDB_USAGE = 10;
//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 450;
//! -dbbatchsize default (bytes)
static const int64_t nDefaultDbBatchSize = 16 << 20;
//! max. -dbcache (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB specific cache, if no -txindex (MiB)
static const int64_t nMaxBlockDBCache = 2;
//! Max memory allocated to block tree DB specific cache, if -txindex (MiB)
// Unlike for the UTXO database, for the txindex scenario the leveldb cache make
// a meaningful difference: https://github.com/bitcoin/bitcoin/pull/8273#issuecomment-229601991
static const int64_t nMaxTxIndexCache = 1024;
//! Max memory allocated to coin DB specific cache (MiB)
static const int64_t nMaxCoinsDBCache = 8;

/** CCoinsView backed by the coin database (chainstate/) */
class CCoinsViewDB final : public CCoinsView
{
protected:
    CDBWrapper db;
public:
    explicit CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoin(const COutPoint &outpoint, Coin &coin) const override;
    bool HaveCoin(const COutPoint &outpoint) const override;
    uint256 GetBestBlock() const override;
    std::vector<uint256> GetHeadBlocks() const override;
    bool BatchWrite(CCoinsMap &mapCoins, const uint256 &hashBlock) override;
    CCoinsViewCursor *Cursor() const override;

    //! Attempt to update from an older database format. Returns whether an error occurred.
    bool Upgrade();
    size_t EstimateSize() const override;
};

/** Specialization of CCoinsViewCursor to iterate over a CCoinsViewDB */
class CCoinsViewDBCursor: public CCoinsViewCursor
{
public:
    ~CCoinsViewDBCursor() {}

    bool GetKey(COutPoint &key) const override;
    bool GetValue(Coin &coin) const override;
    unsigned int GetValueSize() const override;

    bool Valid() const override;
    void Next() override;

private:
    CCoinsViewDBCursor(CDBIterator* pcursorIn, const uint256 &hashBlockIn):
        CCoinsViewCursor(hashBlockIn), pcursor(pcursorIn) {}
    std::unique_ptr<CDBIterator> pcursor;
    std::pair<char, COutPoint> keyTmp;

    friend class CCoinsViewDB;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CDBWrapper
{
public:
    explicit CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo &info);
    bool ReadLastBlockFile(int &nFile);
    bool WriteReindexing(bool fReindexing);
    void ReadReindexing(bool &fReindexing);
    bool WriteFlag(const std::string &name, bool fValue);
    bool ReadFlag(const std::string &name, bool &fValue);
    bool LoadBlockIndexGuts(const Consensus::Params& consensusParams, std::function<CBlockIndex*(const uint256&)> insertBlockIndex);

    bool ReadRCTOutput(int64_t i, CAnonOutput &ao);
    bool WriteRCTOutput(int64_t i, const CAnonOutput &ao);
    bool EraseRCTOutput(int64_t i);

    bool ReadRCTOutputLink(const CCmpPubKey &pk, int64_t &i);
    bool WriteRCTOutputLink(const CCmpPubKey &pk, int64_t i);
    bool EraseRCTOutputLink(const CCmpPubKey &pk);

    bool ReadRCTKeyImage(const CCmpPubKey &ki, uint256 &txhash);
    bool WriteRCTKeyImage(const CCmpPubKey &ki, const uint256 &txhash);
    bool EraseRCTKeyImage(const CCmpPubKey &ki);
};

/** Zerocoin database (zerocoin/) */
class CZerocoinDB : public CDBWrapper
{
public:
    explicit CZerocoinDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CZerocoinDB(const CZerocoinDB&);
    void operator=(const CZerocoinDB&);

public:
    /** Write Zerocoin mints to the zerocoinDB in a batch */
    bool WriteCoinMintBatch(const std::map<libzerocoin::PublicCoin, uint256>& mintInfo);
    bool ReadCoinMint(const CBigNum& bnPubcoin, uint256& txHash);
    bool ReadCoinMint(const uint256& hashPubcoin, uint256& hashTx);
    bool ReadPubcoinSpend(const uint256& hashPubcoin, uint256& txHash, uint256& hashBlock);
    bool WritePubcoinSpendBatch(std::map<uint256, uint256>& mapPubcoinSpends, const uint256& hashBlock);
    bool ErasePubcoinSpend(const uint256& hashPubcoin);

    /** Write Zerocoin spends to the zerocoinDB in a batch */
    bool WriteCoinSpendBatch(const std::map<libzerocoin::CoinSpend, uint256>& spendInfo);
    bool ReadCoinSpend(const CBigNum& bnSerial, uint256& txHash);
    bool ReadCoinSpend(const uint256& hashSerial, uint256 &txHash);
    bool EraseCoinMint(const CBigNum& bnPubcoin);
    bool EraseCoinSpend(const CBigNum& bnSerial);
    bool WipeCoins(std::string strType);
    bool WriteAccumulatorValue(const uint256& nChecksum, const CBigNum& bnValue);
    bool ReadAccumulatorValue(const uint256& nChecksum, CBigNum& bnValue);
    bool EraseAccumulatorValue(const uint256& nChecksum);

    /** blacklist **/
    bool WriteBlacklistedOutpoint(const COutPoint& outpoint, int nType);
    bool EraseBlacklistedOutpoint(const COutPoint& outpoint);
    bool WriteBlacklistedPubcoin(const uint256& hashPubcoin);
    bool EraseBlacklisterPubcoin(const uint256& hashPubcoin);
    bool LoadBlacklistOutPoints();
    bool LoadBlacklistPubcoins();
};

#endif // BITCOIN_TXDB_H
