/*---------------------------------------------------------------------------------------------
*  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/pontoon-c
*  Licensed under the MIT License. See LICENSE in the project root for license information.
*--------------------------------------------------------------------------------------------*/
#include <inttypes.h>
#include <pontoon/pontoon.h>
#include <stdbool.h>

static PontoonMilliseconds trueRandomDuration(
    PontoonMilliseconds minimum, PontoonMilliseconds maximum)
{
    size_t range = maximum - minimum;

    return minimum + (PontoonMilliseconds)(rand() % (int)range);
}

static void resetTimeoutAndRandomizeThreshold(PontoonTimeout* timeout)
{
    timeout->value = 0;
    timeout->threshold = trueRandomDuration(timeout->min, timeout->max);
}

static void disableTimeout(PontoonTimeout* timeout)
{
    timeout->threshold = SIZE_MAX;
    timeout->value = 0;
}

static void resetTimeout(PontoonTimeout* timeout)
{
    timeout->value = 0;
}

static void updateTimeout(PontoonTimeout* timeout, PontoonMilliseconds timePassed)
{
    timeout->value += timePassed;
}

bool pontoonTimeoutHasExpired(PontoonTimeout* timeout)
{
    return timeout->value >= timeout->threshold;
}

static void electionInit(PontoonElection* election, Clog log)
{
    election->log = log;
    election->ballotCount = 0;
    election->term = PONTOON_EMPTY_TERM_ID;
    election->electionTimeout.min = 100;
    election->electionTimeout.max = 300;
    disableTimeout(&election->electionTimeout);
}

void pontoonInit(Pontoon* self, PontoonNodeId nodeId, Clog log)
{
    self->log = log;
    self->id = nodeId;
    self->leaderNodeId = PONTOON_NODE_EMPTY_ID;
    self->nodeCount = 0;
    self->type = PontoonTypeSupporter;
    self->knowledge = 0;

    self->lastYesVote.votedForNodeId = PONTOON_NODE_EMPTY_ID;
    self->lastYesVote.term = PONTOON_EMPTY_TERM_ID;

    self->heartBeatTimeout.min = 100;
    self->heartBeatTimeout.max = 300;
    resetTimeoutAndRandomizeThreshold(&self->heartBeatTimeout);

    electionInit(&self->election, log);
}

PontoonExternalNode* pontoonAddNode(Pontoon* self, PontoonNodeId nodeId)
{
    CLOG_ASSERT(self->nodeCount < PONTOON_MAX_NODE_COUNT, "ran out of nodes")
    PontoonExternalNode* node = &self->nodes[self->nodeCount++];
    node->id = nodeId;
    return node;
}

static void startNewTermAndAnnounceMyselfAsCandidate(Pontoon* self)
{
    // Lost connection to leader, I propose a vote for myself as a candidate
    self->type = PontoonTypeCandidate;
    self->term++;
    CLOG_C_DEBUG(&self->log,
        "I haven't received any heartbeats from the leader for a while. I start a new election "
        "term %u with myself (%zu) as candidate",
        self->term, self->id)
}

static void electionStartDetected(Pontoon* self)
{
    resetTimeoutAndRandomizeThreshold(&self->election.electionTimeout);
    CLOG_C_DEBUG(&self->log, "an election (%u) has been detected, reset election timeout (%zu ms)",
        self->term, self->election.electionTimeout.threshold)
    self->election.term = self->term;
    self->election.registeredToVoteCount = self->nodeCount;
}

static bool isElectionInProgress(const PontoonElection* election)
{
    return election->term != PONTOON_EMPTY_TERM_ID;
}

static void stopTheElection(PontoonElection* election)
{
    election->term = PONTOON_EMPTY_TERM_ID;
    election->ballotCount = 0;
}

static void newTermDetectedBecomeSupporter(Pontoon* self, PontoonTermId term)
{
    CLOG_C_DEBUG(&self->log, "detected a new term (%u), I am on term (%u), become a supporter",
        term, self->term)
    self->lastYesVote.term = PONTOON_EMPTY_TERM_ID;
    self->lastYesVote.votedForNodeId = PONTOON_NODE_EMPTY_ID;
    self->term = term;
    self->type = PontoonTypeSupporter;
}

static void becomeSupporterWithNewLeader(Pontoon* self, PontoonNodeId leaderId)
{
    resetTimeout(&self->heartBeatTimeout);
    CLOG_C_DEBUG(&self->log,
        "accepting a new leader (%zu) reset heartbeat timeout (%zu out of %zu ms)", leaderId,
        self->heartBeatTimeout.value, self->heartBeatTimeout.threshold)
    self->leaderNodeId = leaderId;
    self->type = PontoonTypeSupporter;
}

static void startElection(Pontoon* self)
{
    startNewTermAndAnnounceMyselfAsCandidate(self);
    electionStartDetected(self);
}

PontoonTimePassedDecision pontoonTimePassed(Pontoon* self, PontoonMilliseconds time)
{
    CLOG_C_DEBUG(&self->log, "time passed (%zu ms)", time)

    if (self->type == PontoonTypeLeader) {
        // As a leader, I just send heart beats to all nodes, so no timers are running
        return PontoonTimePassedDecisionWait;
    }

    bool electionTimedOut = false;
    if (isElectionInProgress(&self->election)) {
        updateTimeout(&self->election.electionTimeout, time);
        electionTimedOut = pontoonTimeoutHasExpired(&self->election.electionTimeout);
    }

    updateTimeout(&self->heartBeatTimeout, time);
    if (pontoonTimeoutHasExpired(&self->heartBeatTimeout) || electionTimedOut) {
        startElection(self);
        return PontoonTimePassedDecisionAnnounceMyCandidacyToAllNodes;
    }

    return PontoonTimePassedDecisionWait;
}

static bool electionCheckIfNewVoteAndStoreYesVote(
    PontoonElection* election, const PontoonIncomingVote* incomingVote)
{
    if (incomingVote->term != election->term) {
        CLOG_C_INFO(&election->log, "wrong term (%u) for vote in this election (%u)",
            incomingVote->term, election->term)
        return false;
    }

    for (size_t i = 0; i < election->ballotCount; ++i) {
        if (incomingVote->sentFromNodeId == election->ballots[i].castByNodeId) {
            CLOG_C_NOTICE(&election->log, "node %zu already voted in this election",
                incomingVote->sentFromNodeId)
            return false;
        }
    }

    PontoonYesBallot* yesBallot = &election->ballots[election->ballotCount++];
    yesBallot->castByNodeId = incomingVote->sentFromNodeId;
    CLOG_C_DEBUG(&election->log, "node %zu added ballot with 'yes' in this election (%u)",
        incomingVote->sentFromNodeId, election->term)
    election->receivedYesVoteCount++;

    return true;
}

static bool electionCheckDidIWin(const PontoonElection* election)
{
    size_t totalNodeCountIncludingSelf = (election->registeredToVoteCount + 1);
    size_t threshold = totalNodeCountIncludingSelf / 2;
    CLOG_C_DEBUG(&election->log,
        "ballot accepted, we have %zu yes votes out of %zu, need more than %zu",
        election->receivedYesVoteCount, totalNodeCountIncludingSelf, threshold)

    return election->receivedYesVoteCount > threshold;
}

static void wonElectionAsCandidate(Pontoon* self)
{
    CLOG_C_INFO(&self->log, "I won the election!")
    self->type = PontoonTypeLeader;
    self->leaderNodeId = self->id;
    stopTheElection(&self->election);
}

static bool isKnownNode(Pontoon* self, PontoonNodeId nodeId)
{
    for (size_t i = 0; i < self->nodeCount; ++i) {
        if (self->nodes[i].id == nodeId) {
            return true;
        }
    }

    return false;
}

static const char* castVoteToString(PontoonCastVote yesNo)
{
    switch (yesNo) {
    case PontoonCastVoteYes:
        return "Yes";
    case PontoonCastVoteNo:
        return "No";
    }
}

PontoonReceivedVoteResult pontoonOnVote(Pontoon* self, const PontoonIncomingVote* vote)
{
    CLOG_C_VERBOSE(&self->log, "incoming vote %s from node %zu", castVoteToString(vote->castVote),
        vote->sentFromNodeId)
    if (!isKnownNode(self, vote->sentFromNodeId)) {
        return PontoonReceivedVoteResultUnknownNode;
    }
    // Voting is in a later term, just give up any existing candidacy and become a supporter
    if (vote->term > self->term) {
        newTermDetectedBecomeSupporter(self, vote->term);
        return PontoonReceivedVoteResultConvertedToSupporter;
    }

    if (vote->term < self->term) {
        // Voting for an old term
        return PontoonReceivedVoteResultOldTerm;
    }

    if (self->type == PontoonTypeCandidate && vote->castVote == PontoonCastVoteYes) {
        if (electionCheckIfNewVoteAndStoreYesVote(&self->election, vote)) {
            if (electionCheckDidIWin(&self->election)) {
                CLOG_C_DEBUG(&self->log,
                    "We have a majority of votes, declare myself as winner of the election and new "
                    "leader")
                wonElectionAsCandidate(self);
                return PontoonReceivedVoteResultWonTheElectionTellAllNodes;
            } else {
                CLOG_C_DEBUG(&self->log, "Not enough yes votes so far")
            }
            return PontoonReceivedVoteResultAccepted;
        } else {
            return PontoonReceivedVoteResultAlreadyAccepted;
        }
    }

    CLOG_C_NOTICE(&self->log, "ignoring vote %s from node %zu", castVoteToString(vote->castVote),
        vote->sentFromNodeId)

    return PontoonReceivedVoteResultIgnored;
}

PontoonHeartBeatResult pontoonOnHeartBeatFromProclaimedLeader(
    Pontoon* self, PontoonLeaderHeartBeat proclamationFromLeader)
{
    if (!isKnownNode(self, proclamationFromLeader.receivedFromId)) {
        return PontoonHeartBeatResultUnknownNode;
    }

    if (proclamationFromLeader.term > self->term) {
        newTermDetectedBecomeSupporter(self, proclamationFromLeader.term);
    }

    if (proclamationFromLeader.term == self->term) {
        becomeSupporterWithNewLeader(self, proclamationFromLeader.receivedFromId);
        return PontoonHeartBeatResultAcceptedNewLeader;
    }

    return PontoonHeartBeatResultIsSupporter;
}

PontoonRequestToSupportCandidacyDecision pontoonOnRequestStartElectionAndSupportCandidate(
    Pontoon* self, const PontoonElectionRequestFromCandidate* electionRequest)
{
    // if this is an old request, then just vote no
    if (electionRequest->term < self->term) {
        CLOG_C_NOTICE(&self->log, "was an old term, ignoring start election request")
        return PontoonRequestToSupportCandidacyDecisionOldTerm;
    }

    if (electionRequest->term > self->term) {
        CLOG_C_NOTICE(&self->log,
            "start election request for a term (%u) after the one I know about (%u)",
            electionRequest->term, self->term)
        // The candidate is running in a later term, so if I was a candidate, I can give up my run for a leadership.
        // Also clear the last yes vote, since it is a new term.
        newTermDetectedBecomeSupporter(self, electionRequest->term);
        electionStartDetected(self);
    }

    bool candidateHasSameOrMoreKnowledgeThanUs = electionRequest->knowledge >= self->knowledge;
    bool hasVotedForSomeoneElseThisTerm
        = (self->lastYesVote.votedForNodeId != PONTOON_NODE_EMPTY_ID)
        && (self->lastYesVote.votedForNodeId != electionRequest->proclaimedCandidateNodeId);

    CLOG_C_DEBUG(&self->log,
        "candidateHasEqualOrMoreKnowledge: %d. have we already voted this term: %d. current term "
        "(%u)",
        candidateHasSameOrMoreKnowledgeThanUs, hasVotedForSomeoneElseThisTerm, self->term)

    bool shouldVoteYes = candidateHasSameOrMoreKnowledgeThanUs && !hasVotedForSomeoneElseThisTerm;
    if (shouldVoteYes) {
        // I cast a vote for this candidate, and set myself as a supporter
        self->lastYesVote.votedForNodeId = electionRequest->proclaimedCandidateNodeId;
        self->lastYesVote.term = electionRequest->term;
        CLOG_C_DEBUG(&self->log, "I vote yes for candidate (%zu) in term (%u)",
            self->lastYesVote.votedForNodeId, self->lastYesVote.term)
        self->type = PontoonTypeSupporter;
        return PontoonRequestToSupportCandidacyDecisionSupporter;
    }

    CLOG_C_DEBUG(&self->log, "I vote NO for candidate (%zu) in term (%u)",
        self->lastYesVote.votedForNodeId, self->lastYesVote.term)

    return PontoonRequestToSupportCandidacyDecisionDenied;
}
