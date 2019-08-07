// Copyright (c) 2017-2019 The PIVX developers
// Copyright (c) 2019 The Veil developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <tinyformat.h>
#include <secp256k1/include/secp256k1_mlsag.h>
#include "veil/zerocoin/accumulators.h"
#include "veil/zerocoin/mintmeta.h"
#include "chain.h"
#include "chainparams.h"
#include "wallet/deterministicmint.h"
#include "validation.h"
#include "stakeinput.h"
#include "veil/proofofstake/kernel.h"
#include "veil/ringct/blind.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#include "veil/ringct/anonwallet.h"
#endif

typedef std::vector<unsigned char> valtype;

ZerocoinStake::ZerocoinStake(const libzerocoin::CoinSpend& spend)
{
    this->nChecksum = spend.getAccumulatorChecksum();
    this->denom = spend.getDenomination();
    uint256 nSerial = spend.getCoinSerialNumber().getuint256();
    this->hashSerial = Hash(nSerial.begin(), nSerial.end());
    this->pindexFrom = nullptr;
    fMint = false;
    m_type = STAKE_ZEROCOIN;
}

int ZerocoinStake::GetChecksumHeightFromMint()
{
    int nHeightChecksum = chainActive.Height() + 1 - Params().Zerocoin_RequiredStakeDepth();
    //Need to return the first occurance of this checksum in order for the validation process to identify a specific
    //block height
    uint256 nChecksum;
    nChecksum = chainActive[nHeightChecksum]->mapAccumulatorHashes[denom];
    return GetChecksumHeight(nChecksum, denom);
}

int ZerocoinStake::GetChecksumHeightFromSpend()
{
    return GetChecksumHeight(nChecksum, denom);
}

uint256 ZerocoinStake::GetChecksum()
{
    return nChecksum;
}

// The Zerocoin block index is the first appearance of the accumulator checksum that was used in the spend
// note that this also means when staking that this checksum should be from a block that is beyond 60 minutes old and
// 100 blocks deep.
CBlockIndex* ZerocoinStake::GetIndexFrom()
{
    if (pindexFrom)
        return pindexFrom;

    int nHeightChecksum = 0;

    if (fMint)
        nHeightChecksum = GetChecksumHeightFromMint();
    else
        nHeightChecksum = GetChecksumHeightFromSpend();

    if (nHeightChecksum > chainActive.Height()) {
        pindexFrom = nullptr;
    } else {
        //note that this will be a nullptr if the height DNE
        pindexFrom = chainActive[nHeightChecksum];
    }

    return pindexFrom;
}

CAmount ZerocoinStake::GetValue()
{
    return denom * COIN;
}

int ZerocoinStake::HeightToModifierHeight(int nHeight)
{
    //Nearest multiple of KernelModulus that is over KernelModulus bocks deep in the chain
    return (nHeight - Params().KernelModulus()) - (nHeight % Params().KernelModulus()) ;
}

//Use the first accumulator checkpoint that occurs 60 minutes after the block being staked from
bool ZerocoinStake::GetModifier(uint64_t& nStakeModifier)
{
    CBlockIndex* pindex = GetIndexFrom();
    if (!pindex)
        return false;

    int nNearest100Block = ZerocoinStake::HeightToModifierHeight(pindex->nHeight);

    //Rare case block index < 100, we don't use proof of stake for these blocks
    if (nNearest100Block < 1) {
        nStakeModifier = 1;
        return false;
    }

    while (nNearest100Block != pindex->nHeight) {
        pindex = pindex->pprev;
    }

    nStakeModifier = UintToArith256(pindex->mapAccumulatorHashes[denom]).GetLow64();
    return true;
}

CDataStream ZerocoinStake::GetUniqueness()
{
    //The unique identifier for a Zerocoin VEIL is a hash of the serial
    CDataStream ss(SER_GETHASH, 0);
    ss << hashSerial;
    return ss;
}

bool ZerocoinStake::CreateTxIn(CWallet* pwallet, CTxIn& txIn, uint256 hashTxOut)
{
#ifdef ENABLE_WALLET
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
#endif
        return error("%s: wallet disabled", __func__);
#ifdef ENABLE_WALLET
    }

    CBlockIndex* pindexCheckpoint = GetIndexFrom();
    if (!pindexCheckpoint)
        return error("%s: failed to find checkpoint block index", __func__);

    CZerocoinMint mint;
    if (!pwallet->GetMintFromStakeHash(hashSerial, mint))
        return error("%s: failed to fetch mint associated with serial hash %s", __func__, hashSerial.GetHex());

    if (libzerocoin::ExtractVersionFromSerial(mint.GetSerialNumber()) < 2)
        return error("%s: serial extract is less than v2", __func__);

    int nSecurityLevel = 100;
    CZerocoinSpendReceipt receipt;
    if (!pwallet->MintToTxIn(mint, nSecurityLevel, hashTxOut, txIn, receipt, libzerocoin::SpendType::STAKE, GetIndexFrom()))
        return error("%s\n", receipt.GetStatusMessage());

    return true;
#endif
}

bool ZerocoinStake::CreateTxOuts(CWallet* pwallet, vector<CTxOut>& vout, CAmount nTotal)
{
#ifdef ENABLE_WALLET
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
#endif
        return error("%s: wallet disabled", __func__);
#ifdef ENABLE_WALLET
    }

    //Create an output returning the Zerocoin VEIL that was staked
    CTxOut outReward;
    libzerocoin::CoinDenomination denomStaked = libzerocoin::AmountToZerocoinDenomination(this->GetValue());
    CDeterministicMint dMint;
    if (!pwallet->CreateZOutPut(denomStaked, outReward, dMint))
        return error("%s: failed to create zerocoin output", __func__);
    vout.emplace_back(outReward);

    //Add new staked denom to our wallet
    if (!pwallet->DatabaseMint(dMint))
        return error("%s: failed to database the staked Zerocoin", __func__);

    CAmount nRewardOut = 0;
    while (nRewardOut < nTotal) {
        CTxOut out;
        CDeterministicMint dMintReward;
        auto denomReward = libzerocoin::CoinDenomination::ZQ_TEN;
        if (!pwallet->CreateZOutPut(denomReward, out, dMintReward))
            return error("%s: failed to create Zerocoin output", __func__);
        vout.emplace_back(out);

        if (!pwallet->DatabaseMint(dMintReward))
            return error("%s: failed to database mint reward", __func__);
        nRewardOut += libzerocoin::ZerocoinDenominationToAmount(denomReward);
    }

    return true;
#endif
}

bool ZerocoinStake::GetTxFrom(CTransaction& tx)
{
    return false;
}

bool ZerocoinStake::MarkSpent(CWallet *pwallet, const uint256& txid)
{
#ifdef ENABLE_WALLET
    if (gArgs.GetBoolArg("-disablewallet", DEFAULT_DISABLE_WALLET)) {
#endif
        return error("%s: wallet disabled", __func__);
#ifdef ENABLE_WALLET
    }

    CzTracker* zTracker = pwallet->GetZTrackerPointer();
    CMintMeta meta;
    if (!zTracker->GetMetaFromStakeHash(hashSerial, meta))
        return error("%s: tracker does not have serialhash", __func__);

    zTracker->SetPubcoinUsed(meta.hashPubcoin, txid);
    return true;
#endif
}

uint256 ZerocoinStake::GetSerialStakeHash()
{
    return hashSerial;
}

RingCtStakeCandidate::RingCtStakeCandidate(CWallet* pwallet, CTransactionRef ptx, const COutPoint& outpoint, const COutputRecord* pout) : m_outpoint(outpoint), m_pout(pout)
{
    m_tx = ptx;

    AnonWallet* panonwallet = pwallet->GetAnonWallet();
    AnonWalletDB wdb(pwallet->GetDBHandle());

    //Get key image
    CCmpPubKey keyimage;
    if (!wdb.GetKeyImageFromOutpoint(m_outpoint, keyimage)) {
        //Manually get the key image if possible.
        CKeyID idStealth;
        if (!m_pout->GetStealthID(idStealth)) {
            error("%s:%d FAILED TO GET STEALTH ID FOR RCTCANDIDATE\n", __func__, __LINE__);
            throw std::runtime_error("rct candidate failed.");
        }

        CKey keyStealth;
        if (!panonwallet->GetKey(idStealth, keyStealth))
            throw std::runtime_error("RingCtStakeCandidate failed to get stealth key");

        CTxOutRingCT* txout = (CTxOutRingCT*)m_tx->vpout[m_outpoint.n].get();
        if (secp256k1_get_keyimage(secp256k1_ctx_blind, keyimage.ncbegin(), txout->pk.begin(), keyStealth.begin()) != 0)
            throw std::runtime_error("Unable to get key image");

        if (!wdb.WriteKeyImageFromOutpoint(m_outpoint, keyimage))
            error("%s: failed to write keyimage to disk.");
    }

    m_hashPubKey = keyimage.GetHash();
    m_nAmount = m_pout->GetAmount();
}

// Uniqueness is the key image associated with this input
CDataStream RingCtStakeCandidate::GetUniqueness()
{
    CDataStream ss(0,0);
    ss << m_hashPubKey;
    return ss;
}


//Indexfrom is the same for all rct candidates and is based on the current index minus a certain amount of blocks
CBlockIndex* RingCtStakeCandidate::GetIndexFrom()
{
    int nCurrentHeight = chainActive.Height();
    return chainActive.Tip()->GetAncestor(nCurrentHeight + 1 - Params().Zerocoin_RequiredStakeDepth());
}

PublicRingCtStake::PublicRingCtStake(const CTxIn& txin, const CTxOutRingCT* pout) : m_txin(txin)
{
    //Extract the pubkeyhash from the keyimage
    const std::vector<uint8_t> vKeyImages = txin.scriptData.stack[0];
    uint32_t nInputs, nRingSize;
    txin.GetAnonInfo(nInputs, nRingSize);
    const CCmpPubKey &ki = *((CCmpPubKey*)&vKeyImages[0]); //todo: have check that there is only one keyimage in a stake tx, should occur before this constructor, or else move the loading of the key image to not be in constructor
    m_hashPubKey = ki.GetHash();

    int nExp = 0;
    int nMantissa = 0;
    CAmount nMinValue = 0;
    CAmount nMaxValue = 0;
    GetRangeProofInfo(pout->vRangeproof, nExp, nMantissa, nMinValue, nMaxValue);
    m_nMinimumValue = nMinValue;

    m_type = STAKE_RINGCT;
}
