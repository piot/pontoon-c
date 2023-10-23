/*---------------------------------------------------------------------------------------------
*  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/pontoon-c
*  Licensed under the MIT License. See LICENSE in the project root for license information.
*--------------------------------------------------------------------------------------------*/
#ifndef PONTOON_H
#define PONTOON_H

#include <clog/clog.h>
#include <stdbool.h>

#define PONTOON_NODE_EMPTY_ID (SIZE_MAX)
#define PONTOON_EMPTY_TERM_ID (0)

typedef enum PontoonType {
    PontoonTypeSupporter, // You start as a supporter but with an unknown leader. Never sends anything on their own.
    PontoonTypeCandidate,
    PontoonTypeLeader,
} PontoonType;

typedef enum PontoonRequestToSupportCandidacyDecision {
    PontoonRequestToSupportCandidacyDecisionDenied,
    PontoonRequestToSupportCandidacyDecisionOldTerm,
    PontoonRequestToSupportCandidacyDecisionSupporter
} PontoonRequestToSupportCandidacyDecision;

typedef enum PontoonReceivedVoteResult {
    PontoonReceivedVoteResultAccepted,
    PontoonReceivedVoteResultAlreadyAccepted,
    PontoonReceivedVoteResultOldTerm,
    PontoonReceivedVoteResultIgnored,
    PontoonReceivedVoteResultWonTheElectionTellAllNodes,
    PontoonReceivedVoteResultConvertedToSupporter,
    PontoonReceivedVoteResultUnknownNode
} PontoonReceivedVoteResult;

typedef enum PontoonHeartBeatResult {
    PontoonHeartBeatResultUnknownNode,
    PontoonHeartBeatResultAcceptedNewLeader,
    PontoonHeartBeatResultIsSupporter
} PontoonHeartBeatResult;

typedef size_t PontoonMilliseconds;
typedef uint64_t PontoonKnowledge;
typedef uint32_t PontoonTermId;
typedef size_t PontoonNodeId;

typedef struct PontoonTimeout {
    PontoonMilliseconds value;
    PontoonMilliseconds threshold;
    PontoonMilliseconds max;
    PontoonMilliseconds min;
} PontoonTimeout;

bool pontoonTimeoutHasExpired(PontoonTimeout* timeout);

typedef enum PontoonCastVote { PontoonCastVoteNo, PontoonCastVoteYes } PontoonCastVote;

typedef struct PontoonIncomingVote {
    PontoonNodeId sentFromNodeId;
    PontoonTermId term;
    PontoonCastVote castVote;
} PontoonIncomingVote;

typedef struct PontoonLocalYesVote {
    PontoonTermId term;
    PontoonNodeId votedForNodeId;
} PontoonLocalYesVote;

typedef struct PontoonElectionRequestFromCandidate {
    PontoonNodeId proclaimedCandidateNodeId;
    PontoonTermId term;
    PontoonKnowledge knowledge;
} PontoonElectionRequestFromCandidate;

#define PONTOON_MAX_NODE_COUNT (64)

typedef struct PontoonYesBallot {
    PontoonNodeId castByNodeId;
} PontoonYesBallot;

typedef struct PontoonElection {
    PontoonTermId term;
    PontoonYesBallot ballots[PONTOON_MAX_NODE_COUNT];
    size_t ballotCount;
    size_t registeredToVoteCount;
    size_t receivedYesVoteCount;
    PontoonTimeout electionTimeout;
    Clog log;
} PontoonElection;

typedef struct PontoonExternalNode {
    PontoonNodeId id;
} PontoonExternalNode;

typedef struct PontoonNode {
    PontoonNodeId id;
    PontoonType type;
    PontoonElection election;
    PontoonTermId term;
    PontoonLocalYesVote lastYesVote;

    PontoonTimeout heartBeatTimeout;

    PontoonNodeId leaderNodeId;
    PontoonKnowledge knowledge;
    PontoonExternalNode nodes[PONTOON_MAX_NODE_COUNT];
    size_t nodeCount;
    Clog log;
} Pontoon;

typedef struct PontoonLeaderHeartBeat {
    PontoonTermId term;
    PontoonNodeId receivedFromId;
} PontoonLeaderHeartBeat;

typedef enum PontoonTimePassedDecision {
    PontoonTimePassedDecisionWait,
    PontoonTimePassedDecisionAnnounceMyCandidacyToAllNodes,
} PontoonTimePassedDecision;

void pontoonInit(Pontoon* self, PontoonNodeId nodeId, Clog log);
PontoonExternalNode* pontoonAddNode(Pontoon* self, PontoonNodeId nodeId);
PontoonTimePassedDecision pontoonTimePassed(Pontoon* self, PontoonMilliseconds timePassed);
PontoonRequestToSupportCandidacyDecision pontoonOnRequestStartElectionAndSupportCandidate(
    Pontoon* self, const PontoonElectionRequestFromCandidate* electionRequest);
PontoonReceivedVoteResult pontoonOnVote(Pontoon* self, const PontoonIncomingVote* vote);
PontoonHeartBeatResult pontoonOnHeartBeatFromProclaimedLeader(
    Pontoon* self, PontoonLeaderHeartBeat proclamation);

#endif
