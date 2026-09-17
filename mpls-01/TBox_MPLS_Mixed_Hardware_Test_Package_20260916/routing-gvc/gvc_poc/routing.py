"""Bounded synthetic path selection, not Babel, reservation or production FRR.

The objective is primary-preferred: inspect simple paths in (cost, link-ID
sequence) order and choose the first primary having a compatible shortest
backup. This is not minimum summed pair cost. Search exhaustion is explicitly
INDETERMINATE; negative conclusions require exhaustion or a proved graph cut.
"""

import heapq
import itertools
import math
from typing import Iterator

from .model import Constraints, FailureSet, Link, Path, RouteResult, Topology


class _SearchLimit(Exception):
    pass


class _Budget:
    """Bound popped states and combined queued states across nested searches."""

    def __init__(self, limit: int) -> None:
        self.limit = limit
        self.expansions = 0
        self.queued = 0

    def enqueue(self) -> None:
        if self.queued >= self.limit:
            raise _SearchLimit("combined frontier-state budget exhausted")
        self.queued += 1

    def expand(self) -> None:
        if self.expansions >= self.limit:
            raise _SearchLimit("search expansion budget exhausted")
        self.expansions += 1
        self.queued -= 1


def _physical(link: Link) -> str:
    return link.physical_id if link.physical_id is not None else link.id


def _eligible(topology: Topology, constraints: Constraints, failures: FailureSet):
    active = {
        node.id for node in topology.nodes.values()
        if node.id not in failures.nodes
        and node.site_id not in failures.sites
        and not node.risk_groups.intersection(failures.risks)
    }
    adjacency: dict[str, list[Link]] = {node_id: [] for node_id in active}
    for link in sorted(topology.links.values(), key=lambda value: value.id):
        if not link.usable or link.source not in active or link.target not in active:
            continue
        # Link failures name directed link IDs, not physical_id groups.
        if link.id in failures.links or link.risk_groups.intersection(failures.risks):
            continue
        if constraints.min_rate_bps > 0 and (
            link.capacity_bps is None or link.capacity_bps < constraints.min_rate_bps
        ):
            continue
        if constraints.max_delay_ms is not None and link.delay_ms is None:
            continue
        adjacency[link.source].append(link)
    return adjacency


def _lower_bounds(adjacency, target):
    """Reachability plus exact A* potentials where integer arithmetic is safe.

    Fractional/very large costs use zero potentials to preserve the specified
    floating sum/tie ordering. Their search is still explicitly bounded.
    """
    links = [link for outgoing in adjacency.values() for link in outgoing]
    exact = all(link.cost.is_integer() for link in links)
    exact = exact and 2 * len(adjacency) * max((int(link.cost) for link in links), default=0) <= 2**53
    reverse = {node_id: [] for node_id in adjacency}
    for link in links:
        reverse[link.target].append((link.source, link.cost if exact else 0.0))
    distances = {target: 0.0}
    heap = [(0.0, target)]
    while heap:
        distance, node_id = heapq.heappop(heap)
        if distance != distances[node_id]:
            continue
        for predecessor, cost in reverse[node_id]:
            candidate = distance + cost
            if predecessor not in distances or candidate < distances[predecessor]:
                distances[predecessor] = candidate
                heapq.heappush(heap, (candidate, predecessor))
    return distances


def _paths(
    adjacency: dict[str, list[Link]], source: str, target: str,
    constraints: Constraints, budget: _Budget,
) -> Iterator[Path]:
    """Bounded A*/uniform-cost enumeration; no unsafe node-only dominance."""
    if source not in adjacency or target not in adjacency:
        return
    remaining = _lower_bounds(adjacency, target)
    if source not in remaining:
        return
    max_hops = len(adjacency) - 1
    if constraints.max_hops is not None:
        max_hops = min(max_hops, constraints.max_hops)
    if max_hops <= 0:
        return
    serial = itertools.count()
    heap = []

    def push(cost, ids, nodes, links, delay):
        budget.enqueue()
        heapq.heappush(heap, (cost + remaining[nodes[-1]], ids, next(serial), cost, nodes, links, delay))

    try:
        push(0.0, (), (source,), (), 0.0)
        while heap:
            budget.expand()
            _, ids, _, cost, nodes, links, delay = heapq.heappop(heap)
            if nodes[-1] == target:
                yield Path(nodes, links, cost, delay)
                continue
            if len(links) >= max_hops:
                continue
            for link in adjacency[nodes[-1]]:
                if link.target in nodes or link.target not in remaining:
                    continue
                new_cost = cost + link.cost
                new_delay = None if delay is None or link.delay_ms is None else delay + link.delay_ms
                if constraints.max_delay_ms is not None and (
                    new_delay is None or new_delay > constraints.max_delay_ms
                ):
                    continue
                if not math.isfinite(new_cost) or (new_delay is not None and not math.isfinite(new_delay)):
                    raise _SearchLimit("path arithmetic exceeds finite numeric range")
                push(new_cost, ids + (link.id,), nodes + (link.target,), links + (link,), new_delay)
    finally:
        budget.queued -= len(heap)


def _backup_graph(topology: Topology, adjacency, primary: Path, diversity: str):
    physical = {_physical(link) for link in primary.links}
    internal = set(primary.nodes[1:-1]) if diversity != "link" else set()
    risks: set[str] = set()
    sites: set[str] = set()
    if diversity == "shared_risk":
        for link in primary.links:
            risks.update(link.risk_groups)
        for node_id in internal:
            node = topology.nodes[node_id]
            risks.update(node.risk_groups)
            if node.site_id is not None:
                sites.add(node.site_id)
    endpoints = {primary.nodes[0], primary.nodes[-1]}
    allowed = set()
    for node_id in adjacency:
        if node_id in internal:
            continue
        node = topology.nodes[node_id]
        if diversity == "shared_risk" and node_id not in endpoints and (
            node.site_id in sites or node.risk_groups.intersection(risks)
        ):
            continue
        allowed.add(node_id)
    result = {node_id: [] for node_id in allowed}
    for node_id in allowed:
        for link in adjacency[node_id]:
            if link.target not in allowed or _physical(link) in physical:
                continue
            if diversity == "shared_risk" and link.risk_groups.intersection(risks):
                continue
            result[node_id].append(link)
    return result


def plan_routes(
    topology: Topology, source: str, target: str,
    constraints: Constraints = Constraints(), diversity: str = "link",
    require_backup: bool = False, failures: FailureSet = FailureSet(),
    max_expansions: int = 50000,
) -> RouteResult:
    """Return candidate paths within a deterministic, globally shared budget.

    Diversity is cumulative: physical link, then internal node, then declared
    link/internal-node risks and internal-node sites. Fixed endpoints are not
    independently protectable and are excluded from pair diversity; their
    actual failure still removes all paths. Shared resource_ids are annotations
    only: this experiment neither reserves nor proves restoration capacity.
    """
    if not isinstance(topology, Topology):
        raise ValueError("topology must be a Topology")
    if not isinstance(source, str) or not isinstance(target, str):
        raise ValueError("source and target must be node IDs")
    if source not in topology.nodes or target not in topology.nodes:
        raise ValueError("source and target must exist in topology")
    if source == target:
        raise ValueError("zero-hop circuits are not supported")
    if not isinstance(constraints, Constraints) or not isinstance(failures, FailureSet):
        raise ValueError("constraints/failures must use the validated model")
    if not isinstance(diversity, str) or diversity not in {"link", "internal_node", "shared_risk"}:
        raise ValueError("unsupported diversity")
    if not isinstance(require_backup, bool):
        raise ValueError("require_backup must be boolean")
    if isinstance(max_expansions, bool) or not isinstance(max_expansions, int) or max_expansions <= 0:
        raise ValueError("max_expansions must be a positive integer")
    adjacency = _eligible(topology, constraints, failures)
    if source not in adjacency or target not in adjacency:
        return RouteResult("NO_PATH", reason="source or target is removed by declared failures")
    budget = _Budget(max_expansions)
    endpoint_cut = (
        len({_physical(link) for link in adjacency[source]}) < 2
        or len({_physical(link) for outgoing in adjacency.values() for link in outgoing if link.target == target}) < 2
    )
    first_primary = None
    candidates = _paths(adjacency, source, target, constraints, budget)
    try:
        for primary in candidates:
            if first_primary is None:
                first_primary = primary
            if not require_backup:
                return RouteResult("PRIMARY_ONLY", primary, reason="shortest feasible synthetic path", expansions=budget.expansions)
            if endpoint_cut:
                return RouteResult("UNPROTECTED", primary, reason="proved source/target physical-link bottleneck", expansions=budget.expansions)
            backups = _paths(_backup_graph(topology, adjacency, primary, diversity), source, target, constraints, budget)
            try:
                backup = next(backups, None)
            finally:
                backups.close()
            if backup is not None:
                return RouteResult("PROTECTED_CANDIDATE", primary, backup, "compatible fixed-endpoint paths; capacity and physical protection unqualified", budget.expansions)
        if first_primary is None:
            return RouteResult("NO_PATH", reason="no path satisfies the declared failures and constraints", expansions=budget.expansions)
        return RouteResult("UNPROTECTED", first_primary, reason="exhaustive search found no compatible pair", expansions=budget.expansions)
    except _SearchLimit as exc:
        return RouteResult("INDETERMINATE", first_primary, reason=str(exc), expansions=budget.expansions)
    finally:
        candidates.close()
