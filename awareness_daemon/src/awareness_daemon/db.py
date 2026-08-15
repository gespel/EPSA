
import psycopg2
from psycopg2.extras import RealDictCursor
from contextlib import contextmanager
import logging
from user import EPSAUser
logger = logging.getLogger("awareness_daemon")


class DatabaseHandler:
    """
    Read-only handler for accessing EPSA plugin database.
    Supports querying allocation, execution, and measurement data from PostgreSQL.
    """

    def __init__(self, dsn):
        """
        Initialize database connection parameters.

        Args:
            dsn: libpq connection string, e.g.
                 "host=localhost port=5432 dbname=eps user=eps password=eps"
                 (same format as EPS_DB_CONN_STR used by slurmctld/slurmd)
        """
        self.dsn = dsn
        self.connection = None

    @contextmanager
    def get_connection(self):
        """
        Context manager for database connections.

        Yields:
            psycopg2 connection object
        """
        try:
            conn = psycopg2.connect(self.dsn)
            yield conn
        except psycopg2.Error as e:
            logger.error(f"Database connection error: {e}")
            raise
        finally:
            if conn:
                conn.close()

    def check_connection(self):
        """
        Test database connectivity.
        
        Returns:
            bool: True if connection successful, False otherwise
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor() as cur:
                    cur.execute("SELECT 1")
                    logger.info("Database connection successful")
                    return True
        except Exception as e:
            logger.error(f"Connection check failed: {e}")
            return False

    def get_userid_by_username(self, username):
        """
        Get user ID by username.
        
        Args:
            username: Username string
            
        Returns:
            int: User ID, or None if username not found
        """
        query = """
            SELECT userid
            FROM users
            WHERE username = %s
            LIMIT 1
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (username,))
                    result = cur.fetchone()
                    if result:
                        userid = result['userid']
                        logger.debug(f"Found userid {userid} for username {username}")
                        return userid
                    logger.warning(f"Username '{username}' not found in database")
                    return None
        except Exception as e:
            logger.error(f"Error retrieving userid for username '{username}': {e}")
            return None

    def get_allocation_data(self, jobid):
        """
        Get allocation data for a specific job.
        
        Args:
            jobid: Job ID
            
        Returns:
            dict: Allocation data with keys (jobid, jobname, userid, username, nnodes, ts)
        """
        query = """
            SELECT jobid, jobname, userid, username, nnodes, ts
            FROM allocations
            WHERE jobid = %s
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (jobid,))
                    result = cur.fetchone()
                    if result:
                        logger.debug(f"Retrieved allocation data for job {jobid}")
                    return dict(result) if result else None
        except Exception as e:
            logger.error(f"Error retrieving allocation data: {e}")
            return None

    def get_execution_data(self, jobid):
        """
        Get execution data for a specific job across all nodes.
        
        Args:
            jobid: Job ID
            
        Returns:
            list: List of dicts with keys (execution_id, jobid, nodename, nodeid, tstart, tend)
        """
        query = """
            SELECT id as execution_id, jobid, node_name as nodename,
                   node_id as nodeid, userid, ts_start as tstart, ts_end as tend
            FROM executions
            WHERE jobid = %s
            ORDER BY node_id
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (jobid,))
                    results = cur.fetchall()
                    logger.debug(f"Retrieved {len(results)} execution records for job {jobid}")
                    return [dict(row) for row in results]
        except Exception as e:
            logger.error(f"Error retrieving execution data: {e}")
            return []

    def get_measurement_data(self, execution_id):
        """
        Get all measurements for a specific execution.
        
        Args:
            execution_id: Execution ID
            
        Returns:
            list: List of dicts with measurement data
        """
        query = """
            SELECT exec_id as execution_id, userid, device_name, device_uid, device_type,
                   e0, e1, t0, t1, utilization
            FROM measurements
            WHERE exec_id = %s
            ORDER BY device_name
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (execution_id,))
                    results = cur.fetchall()
                    logger.debug(f"Retrieved {len(results)} measurements for execution {execution_id}")
                    return [dict(row) for row in results]
        except Exception as e:
            logger.error(f"Error retrieving measurement data: {e}")
            return []

    def get_total_energy_consumption(self, jobid):
        """
        Get total energy consumption for a job across all devices and executions.
        
        Args:
            jobid: Job ID
            
        Returns:
            float: Total energy in Joules, or None on error
        """
        query = """
            SELECT SUM(m.e1 - m.e0) as total_energy
            FROM measurements m
            JOIN executions e ON m.exec_id = e.id
            WHERE e.jobid = %s
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (jobid,))
                    result = cur.fetchone()
                    if result and result.get('total_energy'):
                        energy = float(result['total_energy'])
                        logger.debug(f"Total energy for job {jobid}: {energy} mJ")
                        return energy
                    return None
        except Exception as e:
            logger.error(f"Error retrieving total energy: {e}")
            return None

    def get_measurement_count(self, jobid):
        """
        Get number of measurements for a job.
        
        Args:
            jobid: Job ID
            
        Returns:
            int: Number of measurements, or 0 on error
        """
        query = """
            SELECT COUNT(*) as count
            FROM measurements m
            JOIN executions e ON m.exec_id = e.id
            WHERE e.jobid = %s
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (jobid,))
                    result = cur.fetchone()
                    count = result['count'] if result else 0
                    logger.debug(f"Measurement count for job {jobid}: {count}")
                    return count
        except Exception as e:
            logger.error(f"Error retrieving measurement count: {e}")
            return 0

    def get_job_summary(self, jobid):
        """
        Get comprehensive summary for a job (allocation + energy + count).
        
        Args:
            jobid: Job ID
            
        Returns:
            dict: Summary with allocation, total_energy, measurement_count
        """
        allocation = self.get_allocation_data(jobid)
        energy = self.get_total_energy_consumption(jobid)
        count = self.get_measurement_count(jobid)
        
        return {
            'jobid': jobid,
            'allocation': allocation,
            'total_energy_joules': energy,
            'measurement_count': count
        }

    def get_all_jobs_of_user(self, userid):
        """
        Get all job IDs for a specific user.
        
        Args:
            userid: User ID
        Returns:
            list: List of job IDs
        """
        query = """
            SELECT DISTINCT jobid
            FROM executions
            WHERE userid = %s
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (userid,))
                    results = cur.fetchall()
                    logger.debug(f"Retrieved {len(results)} jobs for user {userid}")
                    return [row['jobid'] for row in results]
        except Exception as e:
            logger.error(f"Error retrieving jobs for user {userid}: {e}")
            return []

    def get_total_energy_consumption_for_user_in_kwh(self, userid):
        """
        Get total energy consumption for all jobs of a specific user in kWh.
        
        Args:
            userid: User ID
            
        Returns:
            float: Total energy in kWh, or None on error
        """
        total_energy = 0.0
        job_ids = self.get_all_jobs_of_user(userid)
        for jobid in job_ids:
            energy = self.get_total_energy_consumption(jobid)
            if energy is not None:
                total_energy += energy / 3.6e12  # Convert from Joules to kWh
        return total_energy if total_energy > 0 else None

    def get_energy_leaderboard(self):
        """
        Get an aggregated energy-consumption leaderboard for all known users,
        computed in a single query for efficiency.

        Returns:
            list: List of dicts with keys (userid, username, total_energy_kwh,
                  job_count, execution_count), sorted descending by energy use.
        """
        query = """
            SELECT
                u.userid,
                u.username,
                COALESCE(SUM(m.e1 - m.e0), 0) / 3.6e12 AS total_energy_kwh,
                COUNT(DISTINCT e.jobid) AS job_count,
                COUNT(DISTINCT e.id) AS execution_count
            FROM users u
            LEFT JOIN executions e ON e.userid = u.userid
            LEFT JOIN measurements m ON m.exec_id = e.id
            GROUP BY u.userid, u.username
            ORDER BY total_energy_kwh DESC
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query)
                    results = cur.fetchall()
                    logger.debug(f"Retrieved leaderboard with {len(results)} users")
                    return [dict(row) for row in results]
        except Exception as e:
            logger.error(f"Error retrieving energy leaderboard: {e}")
            return []

    def get_jobs_with_energy_for_user(self, userid):
        """
        Get all jobs of a user together with their aggregated energy
        consumption, node count and timestamps in a single query.

        Args:
            userid: User ID

        Returns:
            list: List of dicts with keys (jobid, jobname, nnodes, ts,
                  total_energy_kwh, execution_count), sorted by newest first.
        """
        query = """
            SELECT
                a.jobid,
                a.job_name AS jobname,
                a.nnodes,
                a.ts,
                COALESCE(SUM(m.e1 - m.e0), 0) / 3.6e12 AS total_energy_kwh,
                COUNT(DISTINCT e.id) AS execution_count
            FROM allocations a
            LEFT JOIN executions e ON e.jobid = a.jobid
            LEFT JOIN measurements m ON m.exec_id = e.id
            WHERE a.userid = %s
            GROUP BY a.jobid, a.job_name, a.nnodes, a.ts
            ORDER BY a.ts DESC
        """
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (userid,))
                    results = cur.fetchall()
                    logger.debug(f"Retrieved {len(results)} jobs for user {userid}")
                    return [dict(row) for row in results]
        except Exception as e:
            logger.error(f"Error retrieving jobs with energy for user {userid}: {e}")
            return []

    def get_username_by_userid(self, userid):
        """
        Get username by user ID.

        Args:
            userid: User ID

        Returns:
            str: Username, or None if not found
        """
        query = "SELECT username FROM users WHERE userid = %s LIMIT 1"
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (userid,))
                    result = cur.fetchone()
                    return result['username'] if result else None
        except Exception as e:
            logger.error(f"Error retrieving username for userid '{userid}': {e}")
            return None

    def get_user_by_userid(self, userid):
        """
        Get user information by user ID.

        Args:
            userid: User ID

        Returns:
            EPSAUser: User information as an EPSAUser object, or None if not found
        """
        query = "SELECT userid, username, email FROM users WHERE userid = %s LIMIT 1"
        try:
            with self.get_connection() as conn:
                with conn.cursor(cursor_factory=RealDictCursor) as cur:
                    cur.execute(query, (userid,))
                    result = cur.fetchone()
                    user = EPSAUser(result['userid'], result['username'], result['email']) if result else None
                    return user
        except Exception as e:
            logger.error(f"Error retrieving user info for userid '{userid}': {e}")
            return None