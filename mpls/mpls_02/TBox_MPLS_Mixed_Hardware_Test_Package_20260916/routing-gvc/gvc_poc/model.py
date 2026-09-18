"""Validated, offline-only data contracts for the generic routing experiment.

Values describe a synthetic directed graph, not vendor capabilities or a live
network. Treat a Topology as a snapshot; do not mutate its dictionaries while a
planner is using it. Rates are bit/s and delays are additive synthetic ms.
"""

from dataclasses import dataclass, field
import math
from numbers import Real
from typing import Iterable, Optional


def _identifier(value: str, field_name: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{field_name} must be a nonempty string")
    return value


def _optional_identifier(value: Optional[str], field_name: str) -> Optional[str]:
    return None if value is None else _identifier(value, field_name)


def _number(value: Real, field_name: str) -> float:
    if isinstance(value, bool) or not isinstance(value, Real):
        raise ValueError(f"{field_name} must be a finite nonnegative number")
    try:
        result = float(value)
    except (OverflowError, ValueError) as exc:
        raise ValueError(f"{field_name} must be finite") from exc
    if not math.isfinite(result) or result < 0:
        raise ValueError(f"{field_name} must be a finite nonnegative number")
    return result


def _identifiers(values: Iterable[str], field_name: str) -> frozenset[str]:
    if isinstance(values, (str, bytes)):
        raise ValueError(f"{field_name} must be a collection of identifiers")
    try:
        return frozenset(_identifier(value, field_name) for value in values)
    except TypeError as exc:
        raise ValueError(f"{field_name} must be a collection of identifiers") from exc


@dataclass(frozen=True, slots=True)
class Node:
    id: str
    role: str = "tbox"
    site_id: Optional[str] = None
    risk_groups: frozenset[str] = field(default_factory=frozenset)

    def __post_init__(self) -> None:
        _identifier(self.id, "node.id")
        _identifier(self.role, "node.role")
        _optional_identifier(self.site_id, "node.site_id")
        object.__setattr__(self, "risk_groups", _identifiers(self.risk_groups, "node.risk_groups"))


@dataclass(frozen=True, slots=True)
class Link:
    id: str
    source: str
    target: str
    cost: float = 1.0
    capacity_bps: Optional[float] = None
    delay_ms: Optional[float] = None
    physical_id: Optional[str] = None
    risk_groups: frozenset[str] = field(default_factory=frozenset)
    resource_ids: tuple[str, ...] = ()
    usable: bool = True

    def __post_init__(self) -> None:
        for attribute in ("id", "source", "target"):
            _identifier(getattr(self, attribute), f"link.{attribute}")
        _optional_identifier(self.physical_id, "link.physical_id")
        object.__setattr__(self, "cost", _number(self.cost, "link.cost"))
        for attribute in ("capacity_bps", "delay_ms"):
            value = getattr(self, attribute)
            if value is not None:
                object.__setattr__(self, attribute, _number(value, f"link.{attribute}"))
        object.__setattr__(self, "risk_groups", _identifiers(self.risk_groups, "link.risk_groups"))
        if isinstance(self.resource_ids, (str, bytes)):
            raise ValueError("link.resource_ids must be a collection of identifiers")
        try:
            resources = tuple(_identifier(value, "link.resource_ids") for value in self.resource_ids)
        except TypeError as exc:
            raise ValueError("link.resource_ids must be a collection of identifiers") from exc
        if len(resources) != len(set(resources)):
            raise ValueError("link.resource_ids must not contain duplicates")
        object.__setattr__(self, "resource_ids", resources)
        if not isinstance(self.usable, bool):
            raise ValueError("link.usable must be boolean")


class Topology:
    """Materialize validated node/link iterables; parallel directed links work."""

    def __init__(self, nodes: Iterable[Node], links: Iterable[Link], name: str = "") -> None:
        if not isinstance(name, str):
            raise ValueError("topology.name must be a string")
        self.name = name
        self.nodes: dict[str, Node] = {}
        self.links: dict[str, Link] = {}
        try:
            for node in nodes:
                if not isinstance(node, Node):
                    raise ValueError("topology.nodes must contain Node objects")
                if node.id in self.nodes:
                    raise ValueError(f"duplicate node ID: {node.id}")
                self.nodes[node.id] = node
            for link in links:
                if not isinstance(link, Link):
                    raise ValueError("topology.links must contain Link objects")
                if link.id in self.links:
                    raise ValueError(f"duplicate link ID: {link.id}")
                if link.source not in self.nodes or link.target not in self.nodes:
                    raise ValueError(f"unknown endpoint for link: {link.id}")
                self.links[link.id] = link
        except TypeError as exc:
            raise ValueError("topology nodes and links must be iterable") from exc


@dataclass(frozen=True, slots=True)
class Constraints:
    min_rate_bps: float = 0
    max_delay_ms: Optional[float] = None
    max_hops: Optional[int] = None

    def __post_init__(self) -> None:
        object.__setattr__(self, "min_rate_bps", _number(self.min_rate_bps, "min_rate_bps"))
        if self.max_delay_ms is not None:
            object.__setattr__(self, "max_delay_ms", _number(self.max_delay_ms, "max_delay_ms"))
        if self.max_hops is not None and (
            isinstance(self.max_hops, bool)
            or not isinstance(self.max_hops, int)
            or self.max_hops < 0
        ):
            raise ValueError("max_hops must be a nonnegative integer or None")


@dataclass(frozen=True, slots=True)
class FailureSet:
    nodes: frozenset[str] = field(default_factory=frozenset)
    links: frozenset[str] = field(default_factory=frozenset)
    sites: frozenset[str] = field(default_factory=frozenset)
    risks: frozenset[str] = field(default_factory=frozenset)

    def __post_init__(self) -> None:
        for attribute in ("nodes", "links", "sites", "risks"):
            object.__setattr__(self, attribute, _identifiers(getattr(self, attribute), attribute))


@dataclass(frozen=True, slots=True)
class Path:
    nodes: tuple[str, ...]
    links: tuple[Link, ...]
    cost: float
    delay_ms: Optional[float] = None


@dataclass(frozen=True, slots=True)
class RouteResult:
    status: str
    primary: Optional[Path] = None
    backup: Optional[Path] = None
    reason: str = ""
    expansions: int = 0
