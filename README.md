<h1 align="center">
    <img src="docs/images/pontoon.png" width="320" />
</h1>

## Introduction

Simplified consensus algorithm. Inspired by algorithms like Raft, Paxos and Bully.

A node can either be a Supporter, Candidate or Leader.

The leader sends frequent heartbeats to all other nodes in the session.

If a node hasn't heard from the leader in a while, it proclaims itself as a candidate for a new term and sends out an election request to vote for them.

A node receiving an election request from a candidate, votes yes or no. It votes yes only if the proclaimed candidate have the same, or very close, to their own knowledge.

The candidate waits for a yes vote from all nodes. If *all* nodes vote yes, the candidate turns itself into a leader. If someone votes no or a timeout happens, it goes back to being a supporter.

As a candidate, if any newer term is detected, it accepts this fact immediately and steps down to a supporter.
