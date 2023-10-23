#include "minctest.h"
#include <clog/console.h>
#include <pontoon/pontoon.h>

clog_config g_clog;
char g_clog_temp_str[CLOG_TEMP_STR_SIZE];

static void initialize(Pontoon* node)
{
    static Clog log;

    clogInitFromGlobal(&log, "pontoon");

    pontoonInit(node, 42, log);
}

static void initializeWithNodes(Pontoon* node, size_t nodeCount)
{
    static Clog log;

    clogInitFromGlobal(&log, "pontoon");

    pontoonInit(node, 42, log);
    for (size_t i = 0; i < nodeCount; ++i) {
        pontoonAddNode(node, 10 + i);
    }
}

static void testHeartBeatTimeout(void)
{
    CLOG_INFO("testHeartBeatTimeout")
    Pontoon node;

    initialize(&node);
    lequal(PontoonTypeSupporter, node.type);

    PontoonTimePassedDecision decision = pontoonTimePassed(&node, 400);
    lequal(PontoonTimePassedDecisionAnnounceMyCandidacyToAllNodes, decision);
    lequal(PontoonTypeCandidate, node.type);
    lok(pontoonTimeoutHasExpired(&node.heartBeatTimeout));
}

static void testHeartBeatNotTimedOut(void)
{
    CLOG_INFO("testHeartBeatNotTimedOut")
    Pontoon node;

    initialize(&node);
    PontoonTimePassedDecision decision = pontoonTimePassed(&node, 100);
    lequal(PontoonTimePassedDecisionWait, decision);
    lok(!pontoonTimeoutHasExpired(&node.heartBeatTimeout));
}

static void testReceivingANewLeaderHeartBeat(void)
{
    CLOG_INFO("testReceivingANewLeaderHeartBeat")

    Pontoon node;

    initializeWithNodes(&node, 3);

    lok(node.leaderNodeId == PONTOON_NODE_EMPTY_ID);

    PontoonLeaderHeartBeat heartBeat;
    heartBeat.term = node.term;
    heartBeat.receivedFromId = 10;

    PontoonHeartBeatResult result = pontoonOnHeartBeatFromProclaimedLeader(&node, heartBeat);
    lequal(PontoonHeartBeatResultAcceptedNewLeader, result);
}

static void testReceivingANewElectionRightAway(void)
{
    CLOG_INFO("testReceivingANewElectionRightAway")

    Pontoon node;

    initializeWithNodes(&node, 3);

    lok(node.leaderNodeId == PONTOON_NODE_EMPTY_ID);

    PontoonElectionRequestFromCandidate electionRequestFromCandidate;
    electionRequestFromCandidate.term = node.term;
    electionRequestFromCandidate.proclaimedCandidateNodeId = 10;
    electionRequestFromCandidate.knowledge = 100;

    PontoonRequestToSupportCandidacyDecision result
        = pontoonOnRequestStartElectionAndSupportCandidate(&node, &electionRequestFromCandidate);
    lequal(PontoonRequestToSupportCandidacyDecisionSupporter, result);
}

static void testWinningAnElection(void)
{
    CLOG_INFO("testWinningAnElection")

    Pontoon node;

    initializeWithNodes(&node, 3);

    PontoonTimePassedDecision decision = pontoonTimePassed(&node, 400);
    lequal(PontoonTimePassedDecisionAnnounceMyCandidacyToAllNodes, decision);
    lequal(PontoonTypeCandidate, node.type);
    lok(pontoonTimeoutHasExpired(&node.heartBeatTimeout));

    PontoonIncomingVote vote;
    vote.term = node.term;
    vote.sentFromNodeId = 10;
    vote.castVote = PontoonCastVoteYes;

    PontoonReceivedVoteResult receivedVoteResult = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultAccepted, receivedVoteResult);

    PontoonReceivedVoteResult receivedVoteResult2 = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultAlreadyAccepted, receivedVoteResult2);

    vote.sentFromNodeId = 11;
    PontoonReceivedVoteResult receivedVoteResult3 = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultAccepted, receivedVoteResult3);

    vote.sentFromNodeId = 12;
    PontoonReceivedVoteResult receivedVoteResult4 = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultWonTheElectionTellAllNodes, receivedVoteResult4);
}

static void testWinningAnElectionAndSomeoneTakingOver(void)
{
    CLOG_INFO("testWinningAnElectionAndSomeoneTakingOver")

    Pontoon node;

    initializeWithNodes(&node, 4);

    PontoonTimePassedDecision decision = pontoonTimePassed(&node, 400);
    lequal(PontoonTimePassedDecisionAnnounceMyCandidacyToAllNodes, decision);
    lequal(PontoonTypeCandidate, node.type);
    lok(pontoonTimeoutHasExpired(&node.heartBeatTimeout));

    PontoonIncomingVote vote;
    vote.term = node.term;
    vote.sentFromNodeId = 10;
    vote.castVote = PontoonCastVoteYes;

    PontoonReceivedVoteResult receivedVoteResult = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultAccepted, receivedVoteResult);

    PontoonReceivedVoteResult receivedVoteResult2 = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultAlreadyAccepted, receivedVoteResult2);

    vote.sentFromNodeId = 11;
    PontoonReceivedVoteResult receivedVoteResult3 = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultAccepted, receivedVoteResult3);

    vote.sentFromNodeId = 12;
    PontoonReceivedVoteResult receivedVoteResult4 = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultWonTheElectionTellAllNodes, receivedVoteResult4);

    vote.sentFromNodeId = 13;
    vote.term = node.term + 2;
    PontoonReceivedVoteResult receivedVoteResult5 = pontoonOnVote(&node, &vote);
    lequal(PontoonReceivedVoteResultConvertedToSupporter, receivedVoteResult5);
}

int main(int argc, char* argv[])
{
    g_clog.log = clog_console;
    g_clog.level = CLOG_TYPE_VERBOSE;

    CLOG_DEBUG("starting up test")

    (void)argc;
    (void)argv;

    lrun("testHeartBeatTimeout", testHeartBeatTimeout);
    lrun("testHeartBeatNotTimedOut", testHeartBeatNotTimedOut);
    lrun("testWinningAnElection", testWinningAnElection);
    lrun("testWinningAnElectionAndSomeoneTakingOver", testWinningAnElectionAndSomeoneTakingOver);
    lrun("testReceivingANewLeaderHeartBeat", testReceivingANewLeaderHeartBeat);
    lrun("testReceivingANewElectionRightAway", testReceivingANewElectionRightAway);

    lresults();

    return lfails != 0;
}
