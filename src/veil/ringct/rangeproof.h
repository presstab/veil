
#ifndef VEIL_RANGEPROOF_H
#define VEIL_RANGEPROOF_H

#include <uint256.h>
#include <secp256k1.h>
#include <secp256k1/include/secp256k1_rangeproof.h>
#include "ec_commitment.h"

#define DEFAULT_RANGEPROOF_SIZE 5134

//struct secp256k1_context;
//struct secp256k1_pedersen_commitment;
//struct secp256k1_generator;

class RangeProof
{
private:
    std::vector<uint8_t> m_vch;
    uint64_t m_minValue;
    uint64_t m_maxValue;
    int m_exponent;
    int m_bits;
    const char *m_message;
    uint256 m_blind;
    uint256 m_nonce;

    //Secret, known only to creator
    int64_t m_amount;

public:
    RangeProof()
    {
        SetNull();
    }

    RangeProof(secp256k1_context* ctx, const EcCommitment& commitment, const secp256k1_generator* secp256k1_generator_h,
            uint64_t nValue, const uint256& blind, const uint256& nonce, const char* message);

    uint8_t* begin() { return m_vch.data(); }

    void SetNull()
    {
        m_vch.clear();
        m_vch.resize(DEFAULT_RANGEPROOF_SIZE);
        m_minValue = 0;
        m_maxValue = 0;
        m_exponent = 0;
        m_bits = 0;
        m_message = "narration";
        m_amount = 0;
        m_blind = uint256("0x0");
        m_nonce = uint256("0x0");
    }

    void SetParameters(int nMinValue, int nExponent, int nBits)
    {
        m_minValue = nMinValue;
        m_exponent = nExponent;
        m_bits = nBits;
    }

    void SetAmount(const int64_t& nValue) { m_amount = nValue; }
    void SetBlind(const uint256& blind) { m_blind = blind; }
    void SetNonce(const uint256& nonce) { m_nonce = nonce; }

    void TrimSize(size_t size)
    {
        m_vch.resize(size);
    }

    int64_t GetAmount() const { return m_amount; }
    int GetBits() const { return m_bits; }
    uint256 GetBlind() const { return m_nonce; }
    uint8_t* BlindPtr() { return m_blind.begin(); }
    int GetExponent() const { return m_exponent; }
    const unsigned char* GetMessage() const { return (const unsigned char*)m_message; }
    uint64_t GetMax() const { return m_maxValue; }
    uint64_t GetMin() const { return m_minValue; }
    uint256 GetNonce() const { return m_nonce; }
    size_t size() const { return m_vch.size(); }
    std::vector<uint8_t> vch() { return m_vch; }

};

#endif //VEIL_RANGEPROOF_H
