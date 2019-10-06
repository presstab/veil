// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2019 The Veil developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_STAKEINPUT_H
#define PIVX_STAKEINPUT_H

#include "veil/zerocoin/accumulatormap.h"
#include "chain.h"
#include "streams.h"

#include "libzerocoin/CoinSpend.h"

class CKeyStore;
class COutputRecord;
class CTransactionRecord;
class CWallet;
class CWalletTx;

enum StakeInputType
{
    STAKE_ZEROCOIN,
    STAKE_RINGCT,
    STAKE_RINGCTCANDIDATE
};

std::string StakeInputTypeToString(StakeInputType t)
{
    if (t == STAKE_ZEROCOIN) {
        return "ZerocoinStake";
    } else if (t == STAKE_RINGCT) {
        return "RingCtStake";
    } else if (t == STAKE_RINGCTCANDIDATE) {
        return "RingCtStakeCandidate";
    }
    return "error-type";
}

class StakeInput
{
protected:
    CBlockIndex* pindexFrom;
    libzerocoin::CoinDenomination denom = libzerocoin::CoinDenomination::ZQ_ERROR;
    StakeInputType m_type;

public:
    virtual ~StakeInput(){};
    virtual CBlockIndex* GetIndexFrom() = 0;
    virtual bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) = 0;
    virtual bool GetTxFrom(CTransaction& tx) = 0;
    virtual CAmount GetValue() = 0;
    virtual bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) = 0;
    virtual bool GetModifier(uint64_t& nStakeModifier, const CBlockIndex* pindexChainPrev) = 0;
    virtual bool IsZerocoins() = 0;
    virtual CDataStream GetUniqueness() = 0;
    libzerocoin::CoinDenomination GetDenomination() {return denom;}
    StakeInputType GetType() const { return m_type; }
};


// zPIVStake can take two forms
// 1) the stake candidate, which is a zcmint that is attempted to be staked
// 2) a staked zerocoin, which is a zcspend that has successfully staked
class ZerocoinStake : public StakeInput
{
private:
    uint256 nChecksum;
    bool fMint;
    uint256 hashSerial;

public:
    explicit ZerocoinStake(libzerocoin::CoinDenomination denom, const uint256& hashSerial)
    {
        this->denom = denom;
        this->hashSerial = hashSerial;
        this->pindexFrom = nullptr;
        fMint = true;
        m_type = STAKE_ZEROCOIN;
    }

    explicit ZerocoinStake(const libzerocoin::CoinSpend& spend);

    CBlockIndex* GetIndexFrom() override;
    bool GetTxFrom(CTransaction& tx) override;
    CAmount GetValue() override;
    bool GetModifier(uint64_t& nStakeModifier, const CBlockIndex* pindexChainPrev) override;
    CDataStream GetUniqueness() override;
    bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) override;
    bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) override;
    bool MarkSpent(CWallet* pwallet, const uint256& txid);
    bool IsZerocoins() override { return true; }
    int GetChecksumHeightFromMint();
    int GetChecksumHeightFromSpend();
    uint256 GetChecksum();
    uint256 GetSerialStakeHash();

    static int HeightToModifierHeight(int nHeight);
};

//! An input that is used by the owner of the rct input to do the initial stake mining. This contains private information.
class RingCtStakeCandidate : public StakeInput
{
private:
    CTransactionRef m_tx;
    const COutputRecord* m_pout;
    COutPoint m_outpoint;
    uint256 m_hashPubKey;
    CAmount m_nAmount;

public:
    RingCtStakeCandidate(CWallet* pwallet, CTransactionRef ptx, const COutPoint& outpoint, const COutputRecord* pout);

    bool IsZerocoins() override { return false; }
    CBlockIndex* GetIndexFrom() override;
    bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = uint256());
    bool GetTxFrom(CTransaction& tx) { return false; }
    CAmount GetValue() override { return m_nAmount; }
    bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) override { return false; }
    bool GetModifier(uint64_t& nStakeModifier, const CBlockIndex* pindexChainPrev) override { return false; }
    CDataStream GetUniqueness() override;
};

//! A stake that is published to the blockchain and reveals no specific information about the staked input
class PublicRingCtStake : public StakeInput
{
private:
    const CTxIn& m_txin;
    uint256 m_hashPubKey;
    CAmount m_nMinimumValue;

public:
    explicit PublicRingCtStake(const CTxIn& txin, const CTxOutRingCT* pout);
    bool IsZerocoins() override { return false; }

    CBlockIndex* GetIndexFrom() override { return nullptr; }
    bool CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut = uint256()) { return false; }
    bool GetTxFrom(CTransaction& tx) { return false; }
    CAmount GetValue() override { return m_nMinimumValue; }
    bool CreateTxOuts(CWallet* pwallet, std::vector<CTxOut>& vout, CAmount nTotal) override { return false; }
    bool GetModifier(uint64_t& nStakeModifier, const CBlockIndex* pindexChainFrom) override { return false; }
    CDataStream GetUniqueness() { return CDataStream(0,0); }

    //! PublicRingCt specific items
    const std::vector<COutPoint>& GetTxInputs() const;
    CAmount GetMinimumInputValue() const { return m_nMinimumValue; }
};

#endif //PIVX_STAKEINPUT_H
