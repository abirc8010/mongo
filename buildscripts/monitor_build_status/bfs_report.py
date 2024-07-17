from __future__ import annotations

from typing import Any, Dict, List, NamedTuple, Optional, Set

from tabulate import tabulate

from buildscripts.monitor_build_status.evergreen_service import EvgProjectsInfo
from buildscripts.monitor_build_status.jira_service import BfIssue, BfTemperature, TestType


class BFsTemperatureReport(NamedTuple):
    hot: Dict[str, Set[str]]
    cold: Dict[str, Set[str]]
    none: Dict[str, Set[str]]

    @classmethod
    def empty(cls) -> BFsTemperatureReport:
        return cls(hot={}, cold={}, none={})

    def add_bf_data(self, bf: BfIssue) -> None:
        """
        Add BF data to report.

        :param bf: BF issue.
        """
        match bf.temperature:
            case BfTemperature.HOT:
                self._add_bf(self.hot, bf)
            case BfTemperature.COLD:
                self._add_bf(self.cold, bf)
            case BfTemperature.NONE:
                self._add_bf(self.none, bf)

    @staticmethod
    def _add_bf(bf_dict: Dict[str, Set[str]], bf: BfIssue) -> None:
        if bf.assigned_team not in bf_dict:
            bf_dict[bf.assigned_team] = set()
        bf_dict[bf.assigned_team].add(bf.key)


class BFsReport(NamedTuple):
    correctness: BFsTemperatureReport
    performance: BFsTemperatureReport
    unknown: BFsTemperatureReport
    all_assigned_teams: Set[str]

    @classmethod
    def empty(cls) -> BFsReport:
        return cls(
            correctness=BFsTemperatureReport.empty(),
            performance=BFsTemperatureReport.empty(),
            unknown=BFsTemperatureReport.empty(),
            all_assigned_teams=set(),
        )

    def add_bf_data(self, bf: BfIssue, evg_projects_info: EvgProjectsInfo) -> None:
        """
        Add BF data to report.

        :param bf: BF issue.
        :param evg_projects_info: Evergreen project information.
        """
        for evg_project in bf.evergreen_projects:
            if evg_project not in evg_projects_info.active_project_names:
                continue

            self.all_assigned_teams.add(bf.assigned_team)
            test_type = TestType.from_evg_project_name(evg_project)

            match test_type:
                case TestType.CORRECTNESS:
                    self.correctness.add_bf_data(bf)
                case TestType.PERFORMANCE:
                    self.performance.add_bf_data(bf)
                case TestType.UNKNOWN:
                    self.unknown.add_bf_data(bf)

    def get_bf_count(
        self,
        test_types: List[TestType],
        bf_temperatures: List[BfTemperature],
        assigned_team: Optional[str] = None,
    ) -> int:
        """
        Calculate BFs count for a given criteria.

        :param test_types: List of test types (correctness, performance or unknown) criteria.
        :param bf_temperatures: List of BF temperatures (hot, cold or none) criteria.
        :param assigned_team: Assigned team criterion, all teams if None.
        :return: BFs count.
        """
        total_bf_count = 0

        test_type_reports = []
        for test_type in test_types:
            match test_type:
                case TestType.CORRECTNESS:
                    test_type_reports.append(self.correctness)
                case TestType.PERFORMANCE:
                    test_type_reports.append(self.performance)
                case TestType.UNKNOWN:
                    test_type_reports.append(self.unknown)

        bf_temp_reports = []
        for test_type_report in test_type_reports:
            for bf_temperature in bf_temperatures:
                match bf_temperature:
                    case BfTemperature.HOT:
                        bf_temp_reports.append(test_type_report.hot)
                    case BfTemperature.COLD:
                        bf_temp_reports.append(test_type_report.cold)
                    case BfTemperature.NONE:
                        bf_temp_reports.append(test_type_report.none)

        for bf_temp_report in bf_temp_reports:
            if assigned_team is None:
                total_bf_count += sum(len(bfs) for bfs in bf_temp_report.values())
            else:
                total_bf_count += len(bf_temp_report.get(assigned_team, set()))

        return total_bf_count

    def as_dict(self) -> Dict[str, Any]:
        return {
            TestType.CORRECTNESS.value: self.correctness._asdict(),
            TestType.PERFORMANCE.value: self.performance._asdict(),
            TestType.UNKNOWN.value: self.unknown._asdict(),
        }

    def as_str_table(self) -> str:
        """Convert report into string table."""
        headers = ["Assigned Team", "Hot BFs", "Cold BFs", "Perf BFs"]
        table_data = []

        for assigned_team in sorted(self.all_assigned_teams):
            table_data.append(
                [
                    assigned_team,
                    self.get_bf_count(
                        test_types=[TestType.CORRECTNESS],
                        bf_temperatures=[BfTemperature.HOT],
                        assigned_team=assigned_team,
                    ),
                    self.get_bf_count(
                        test_types=[TestType.CORRECTNESS],
                        bf_temperatures=[BfTemperature.COLD, BfTemperature.NONE],
                        assigned_team=assigned_team,
                    ),
                    self.get_bf_count(
                        test_types=[TestType.PERFORMANCE],
                        bf_temperatures=[BfTemperature.HOT, BfTemperature.COLD, BfTemperature.NONE],
                        assigned_team=assigned_team,
                    ),
                ]
            )

        table_data.append(
            [
                "Overall",
                self.get_bf_count(
                    test_types=[TestType.CORRECTNESS],
                    bf_temperatures=[BfTemperature.HOT],
                ),
                self.get_bf_count(
                    test_types=[TestType.CORRECTNESS],
                    bf_temperatures=[BfTemperature.COLD, BfTemperature.NONE],
                ),
                self.get_bf_count(
                    test_types=[TestType.PERFORMANCE],
                    bf_temperatures=[BfTemperature.HOT, BfTemperature.COLD, BfTemperature.NONE],
                ),
            ]
        )

        return tabulate(table_data, headers, tablefmt="outline")
