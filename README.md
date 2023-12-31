<h1 align="center">
    <img src="docs/images/pontoon.png" width="320" />
</h1>

## Introduction

Simplified consensus algorithm. Inspired by algorithms like Raft, Paxos and Bully.

A citizen can either be a Supporter, Candidate or Leader.

The leader sends heart beats to all citizens at regular intervals.

If a supporter hasn't heard from the leader in a while, it proclaims itself as a candidate for a new term and sends out an election request to all other citizens to vote for them.

A citizen receiving an election request from a candidate, votes *yes* or *no*. It votes yes only if the proclaimed candidate have the same, or very close, to their own knowledge. Knowledge, in Pontoon, is an an objectively agreed upon value that determines how worthy the candidate is. The citizen is also only allowed to vote yes for a single candidate in each election.

The candidate waits for votes from all citizens. If *all* citizens vote yes, the candidate turns itself into a leader. If someone votes no or votes has not been received in time, the candidate goes back to being a supporter.

As a candidate or leader, if any newer term is detected, it accepts this fact immediately and steps down to a supporter.

All timeouts are truly random (within a range), to avoid that the same events repeats itself over and over again.
