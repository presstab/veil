#include "rangeproof.h"

#include <secp256k1_rangeproof.h>

RangeProof::RangeProof(secp256k1_context* ctx, const EcCommitment& commitment, const secp256k1_generator* secp256k1_generator_h, uint64_t nValue, const uint256& blind, const uint256& nonce, const char* message)
{
    SetNull();
    m_amount = nValue;
    m_blind = blind;
    m_message = message;
    m_nonce = nonce;

    int ct_exponent = 2;
    int ct_bits = 32;

    auto len = m_vch.size();
    auto* pcommitment = (secp256k1_pedersen_commitment*)commitment.begin();
    secp256k1_rangeproof_sign(ctx, m_vch.data(), &len, m_minValue, pcommitment, blind.begin(), nonce.begin(),
            ct_exponent, ct_bits, nValue, (const unsigned char*) message, std::string(m_message).size(),
            nullptr, 0, secp256k1_generator_h);

    TrimSize(len);
}
