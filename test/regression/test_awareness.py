"""No live database, SMTP endpoint, credentials or Flask listener required."""
import logging
import subprocess
import sys
import unittest
from unittest.mock import MagicMock, patch

import psycopg2
from awareness_daemon.daemon import AwarenessDaemon, SEVEN_DAYS_SECONDS
from awareness_daemon.db import DatabaseHandler
from awareness_daemon.user import EPSAUser


class AwarenessTests(unittest.TestCase):
    def make_worker(self, config):
        return AwarenessDaemon(logging.getLogger("test"), config,
                               MagicMock(), MagicMock(), MagicMock())

    def test_disabled_mail_has_no_side_effects(self):
        for enabled in (False, None, "false", 0):
            with self.subTest(enabled=enabled):
                worker = self.make_worker({"email": {"enabled": enabled}})
                worker.send_email_notifications()
                self.assertEqual(worker.db_handler.mock_calls, [])
                self.assertEqual(worker.mailer.mock_calls, [])
                self.assertEqual(worker.comparator.mock_calls, [])

    def test_missing_email_setting_is_disabled(self):
        worker = self.make_worker({})
        worker.send_email_notifications()
        self.assertEqual(worker.db_handler.mock_calls, [])
        self.assertEqual(worker.mailer.mock_calls, [])

    def test_enabled_email_still_sends(self):
        worker = self.make_worker({"email": {"enabled": True}})
        user = EPSAUser(1, "example", "example@example.org")
        user.total_energy_kwh = 2.0
        user.last_week_energy_kwh = 1.0
        worker.db_handler.get_all_users.return_value = [user]
        worker.comparator.compare_consumption.return_value = ("lamp", "one hour")
        worker.mailer.send_batch.return_value = 1
        worker.send_email_notifications()
        worker.mailer.send_batch.assert_called_once()
        messages = worker.mailer.send_batch.call_args.args[0]
        self.assertEqual(messages[0][0], "example@example.org")

    def test_default_and_configured_intervals(self):
        self.assertEqual(SEVEN_DAYS_SECONDS, 604800)
        self.assertEqual(self.make_worker({}).interval_seconds, 604800)
        self.assertEqual(self.make_worker({"email": {"sending_interval": 300}}).interval_seconds, 300)

    def test_connection_failure_keeps_original_error(self):
        handler = DatabaseHandler("dbname=unused")
        error = psycopg2.OperationalError("synthetic connection failure")
        with patch("awareness_daemon.db.psycopg2.connect", side_effect=error):
            with self.assertLogs("awareness_daemon", level="ERROR"):
                with self.assertRaises(psycopg2.OperationalError) as caught:
                    with handler.get_connection():
                        self.fail("connection unexpectedly opened")
        self.assertIs(caught.exception, error)

    def test_connection_closed_after_success_and_body_error(self):
        handler = DatabaseHandler("dbname=unused")
        for fail in (False, True):
            connection = MagicMock()
            with patch("awareness_daemon.db.psycopg2.connect", return_value=connection):
                if fail:
                    with self.assertRaises(ValueError):
                        with handler.get_connection():
                            raise ValueError("synthetic body failure")
                else:
                    with handler.get_connection() as actual:
                        self.assertIs(actual, connection)
            connection.close.assert_called_once()

    def test_unknown_user_is_not_a_database_error(self):
        connection = MagicMock()
        connection.cursor.return_value.__enter__.return_value.fetchone.return_value = None
        handler = DatabaseHandler("dbname=unused")
        with patch("awareness_daemon.db.psycopg2.connect", return_value=connection):
            with patch("awareness_daemon.db.logger.error") as log_error:
                self.assertIsNone(handler.get_user_by_userid(12345))
                log_error.assert_not_called()
        connection.close.assert_called_once()

    def test_package_import_does_not_initialize_server(self):
        subprocess.run([sys.executable, "-c",
            "import sys; import awareness_daemon; "
            "assert 'awareness_daemon.main' not in sys.modules"], check=True)


if __name__ == "__main__":
    unittest.main()
