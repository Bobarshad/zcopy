"""Candidate single-label MPLS compilation; no reservations or installation."""
from dataclasses import dataclass
from typing import Optional, Tuple
from .model import RouteResult


@dataclass(frozen=True)
class Operation:
    node: str
    action: str
    incoming_label: Optional[int]
    outgoing_label: Optional[int]
    output_link: Optional[str]
    next_hop: Optional[str]


@dataclass(frozen=True)
class CircuitPlan:
    circuit_id: str
    route_status: str
    primary: Tuple[Operation, ...]
    backup: Tuple[Operation, ...]
    installation_state: str = "PLAN_ONLY"


class _PlanLabels:
    def __init__(self, first: int, last: int):
        if type(first) is not int or type(last) is not int:
            raise ValueError("Label bounds must be integers")
        if not 16 <= first <= last <= 1048575:
            raise ValueError("Use a nonreserved label range within 16..1048575")
        self.first, self.last, self.next_by_node = first, last, {}

    def reserve(self, node):
        value = self.next_by_node.get(node, self.first)
        if value > self.last:
            raise ValueError("Simulated label space exhausted at " + node)
        self.next_by_node[node] = value + 1
        return value


def _validate_path(path):
    if len(path.nodes) < 2 or len(path.links) != len(path.nodes) - 1:
        raise ValueError("A circuit needs a complete nonzero-hop path")
    if len(set(path.nodes)) != len(path.nodes):
        raise ValueError("Circuit paths must be loop-free")
    for i, link in enumerate(path.links):
        if (link.source, link.target) != (path.nodes[i], path.nodes[i + 1]):
            raise ValueError("Path node and adjacency sequences disagree")


def _compile_path(path, labels):
    _validate_path(path)
    incoming = {node: labels.reserve(node) for node in path.nodes[1:]}
    actions = []
    for i, node in enumerate(path.nodes):
        last = i == len(path.nodes) - 1
        actions.append(Operation(
            node, "POP" if last else ("PUSH" if i == 0 else "SWAP"),
            None if i == 0 else incoming[node],
            None if last else incoming[path.nodes[i + 1]],
            None if last else path.links[i].id,
            None if last else path.nodes[i + 1],
        ))
    return tuple(actions)


def compile_routes(circuit_id: str, result: RouteResult, *,
                   label_start=16000, label_end=1048575) -> CircuitPlan:
    """Labels are local to this plan; real agent authority is not implemented."""
    if not isinstance(circuit_id, str) or not circuit_id.strip():
        raise ValueError("circuit_id must be a nonempty string")
    if result.status not in {"PRIMARY_ONLY", "UNPROTECTED", "PROTECTED_CANDIDATE"}:
        raise ValueError("No conclusive eligible route plan: " + result.status)
    if result.primary is None:
        raise ValueError("Missing primary")
    _validate_path(result.primary)
    if result.status == "PROTECTED_CANDIDATE":
        if result.backup is None:
            raise ValueError("Protected candidate lacks a backup")
        _validate_path(result.backup)
        if (result.primary.nodes[0], result.primary.nodes[-1]) != (
                result.backup.nodes[0], result.backup.nodes[-1]):
            raise ValueError("A circuit backup must preserve ingress and egress")
        primary_physical = {link.physical_id or link.id for link in result.primary.links}
        backup_physical = {link.physical_id or link.id for link in result.backup.links}
        if primary_physical.intersection(backup_physical):
            raise ValueError("Protected candidate paths must use distinct physical links")
    elif result.backup is not None:
        raise ValueError("Unexpected backup")
    labels = _PlanLabels(label_start, label_end)
    primary = _compile_path(result.primary, labels)
    backup = _compile_path(result.backup, labels) if result.backup is not None else ()
    return CircuitPlan(circuit_id, result.status, primary, backup)
