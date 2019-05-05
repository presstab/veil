#include <secp256k1/include/secp256k1_rangeproof.h>
#include "ec_commitment.h"

EcCommitment::EcCommitment(const secp256k1_context* ctx, const secp256k1_generator* secp256k1_generator_h, const int64_t& nValue, const uint256& blind)
{
    SetNull();
    secp256k1_pedersen_commit(ctx, (secp256k1_pedersen_commitment*)m_vchCommitment.data(), blind.begin(), nValue, secp256k1_generator_h);
}