#include <test/test_veil.h>

#include <boost/test/unit_test.hpp>
#include <iostream>

#include <veil/ringct/blind.h>
#include <veil/ringct/stealth.h>
#include <random.h>
#include <secp256k1_rangeproof.h>
#include <veil/ringct/rangeproof.h>
#include <veil/ringct/ec_commitment.h>

BOOST_AUTO_TEST_SUITE(ringct_tests)


CStealthAddress CreateStealthAddress()
{
    CStealthAddress stealthAddress;
    stealthAddress.SetNull();
    CKey keyScan;
    CKey keySpend;
    keyScan.MakeNewKey(true);
    keySpend.MakeNewKey(true);
    CPubKey pkSpend = keySpend.GetPubKey();
    stealthAddress.scan_pubkey = keyScan.GetPubKey().Raw();
    stealthAddress.spend_pubkey = pkSpend.Raw();
    stealthAddress.scan_secret.Set(keyScan.begin(), true);
    stealthAddress.spend_secret_id = pkSpend.GetID();
    return stealthAddress;
}

//bool CommitToValue(secp256k1_pedersen_commitment* commitment, uint8_t* blind, CAmount nValue)
//{
//    return secp256k1_pedersen_commit(secp256k1_ctx_blind, commitment, blind, nValue, secp256k1_generator_h)==1;
//}



    class CTxOutValueTest
    {
    public:
        EcCommitment commitment;
        std::vector<uint8_t> vchRangeproof;
        std::vector<uint8_t> vchNonceCommitment;
    };


    BOOST_AUTO_TEST_CASE(ct_test)
    {
        RandomInit();
        ECC_Start();
        ECC_Start_Stealth();
        ECC_Start_Blinding();
        SeedInsecureRand();
        secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);

        std::vector<CTxOutValueTest> txins(1);

        std::vector<const uint8_t*> blindptrs;
        uint8_t blindsin[1][32];
        GetRandBytes(&blindsin[0][0], 32);
        blindptrs.push_back(&blindsin[0][0]);

        CAmount nValueIn = 45.69 * COIN;
        uint256 blind_ = Hash(BEGIN(nValueIn), END(nValueIn));

        txins[0].commitment = EcCommitment(secp256k1_ctx_blind, secp256k1_generator_h, nValueIn, blind_);
        //BOOST_CHECK(CommitToValue(&txins[0].commitment, blind_.begin(), nValueIn));
        memcpy(&blindsin[0][0], blind_.begin(), 32);

        const int nTxOut = 2;
        std::vector<CTxOutValueTest> txouts(nTxOut);

        std::vector<RangeProof> vRangeProofs;
        vRangeProofs.resize(2);

        std::vector<CAmount> amount_outs(2);
        amount_outs[0] = 5.69 * COIN;
        amount_outs[1] = 40 * COIN;

        std::vector<CKey> kto_outs(2);
        kto_outs[0].MakeNewKey(true);
        kto_outs[1].MakeNewKey(true);

        std::vector<CPubKey> pkto_outs(2);
        pkto_outs[0] = kto_outs[0].GetPubKey();
        pkto_outs[1] = kto_outs[1].GetPubKey();

        for (size_t k = 0; k < txouts.size(); ++k)
        {
            CTxOutValueTest &txout = txouts[k];
            uint256 blind;
            if (k + 1 == txouts.size())
            {
                // Last to-be-blinded value: compute from all other blinding factors.
                // sum of output blinding values must equal sum of input blinding values
                BOOST_CHECK(secp256k1_pedersen_blind_sum(ctx, blind.begin(), &blindptrs[0], 2, 1));
            } else
            {
                GetRandBytes(blind.begin(), 32);
            };

            txout.commitment = EcCommitment(ctx, secp256k1_generator_h, amount_outs[k], blind);
            //BOOST_CHECK(secp256k1_pedersen_commit(ctx, &txout.commitment, blind.begin(), amount_outs[k], secp256k1_generator_h));

            // Generate ephemeral key for ECDH nonce generation
            CKey ephemeral_key;
            ephemeral_key.MakeNewKey(true);
            CPubKey ephemeral_pubkey = ephemeral_key.GetPubKey();
            txout.vchNonceCommitment.resize(33);
            memcpy(&txout.vchNonceCommitment[0], &ephemeral_pubkey[0], 33);

            // Generate nonce
            uint256 nonce = ephemeral_key.ECDH(pkto_outs[k]);
            CSHA256().Write(nonce.begin(), 32).Finalize(nonce.begin());

            const char *message = "narration";
            //Create rangeproof
            RangeProof rangeproof(ctx, txout.commitment, secp256k1_generator_h, amount_outs[k], blind, nonce, message);
            vRangeProofs[k] = rangeproof;
            blindptrs.emplace_back(rangeproof.BlindPtr());
        };

        std::vector<secp256k1_pedersen_commitment*> vpCommitsIn, vpCommitsOut;
        vpCommitsIn.push_back((secp256k1_pedersen_commitment*)txins[0].commitment.begin());
        vpCommitsOut.push_back((secp256k1_pedersen_commitment*)txouts[0].commitment.begin());
        vpCommitsOut.push_back((secp256k1_pedersen_commitment*)txouts[1].commitment.begin());

        BOOST_CHECK(secp256k1_pedersen_verify_tally(ctx, vpCommitsIn.data(), vpCommitsIn.size(), vpCommitsOut.data(), vpCommitsOut.size()));

        for (size_t k = 0; k < txouts.size(); ++k)
        {
            CTxOutValueTest &txout = txouts[k];

            int rexp;
            int rmantissa;
            uint64_t min_value, max_value;

            RangeProof& rangeproof = vRangeProofs[k];
            BOOST_CHECK(secp256k1_rangeproof_info(ctx, &rexp, &rmantissa, &min_value, &max_value, rangeproof.begin(),
                    rangeproof.size()) == 1);

            min_value = 0;
            max_value = 0;
            BOOST_CHECK(1 == secp256k1_rangeproof_verify(ctx, &min_value, &max_value, (secp256k1_pedersen_commitment*)txout.commitment.begin(),
                    rangeproof.begin(), rangeproof.size(), nullptr, 0, secp256k1_generator_h));

            CPubKey ephemeral_key(txout.vchNonceCommitment);
            BOOST_CHECK(ephemeral_key.IsValid());
            uint256 nonce = kto_outs[k].ECDH(ephemeral_key);
            CSHA256().Write(nonce.begin(), 32).Finalize(nonce.begin());

            uint8_t blindOut[32];
            unsigned char msg[4096];
            size_t msg_size = sizeof(msg);
            uint64_t amountOut;
            BOOST_CHECK(secp256k1_rangeproof_rewind(ctx,
                                                    blindOut, &amountOut, msg, &msg_size, nonce.begin(),
                                                    &min_value, &max_value,
                                                    (secp256k1_pedersen_commitment*)txout.commitment.begin(),
                                                    rangeproof.begin(), rangeproof.size(), nullptr, 0,
                                                    secp256k1_generator_h));

            msg[9] = '\0';
            BOOST_CHECK(memcmp(msg, "narration", 9) == 0);
        };

        secp256k1_context_destroy(ctx);
    }


BOOST_AUTO_TEST_SUITE_END()
