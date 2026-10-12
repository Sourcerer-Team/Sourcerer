-- Syntax highlighting test for SQL: DDL, DML, joins, subqueries, CTEs,
-- window functions, string and numeric literals, operators and comments.
/* A block comment
   spanning several lines. */

CREATE TABLE customers (
    id          INTEGER PRIMARY KEY,
    name        VARCHAR(100) NOT NULL,
    email       TEXT UNIQUE,
    balance     DECIMAL(10, 2) DEFAULT 0.00,
    created_at  TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_customers_name ON customers (name);

INSERT INTO customers (id, name, email, balance)
VALUES (1, 'Ada Lovelace', 'ada@example.com', 150.75),
       (2, 'O''Brien', NULL, -20);

UPDATE customers
SET balance = balance * 1.05, email = LOWER(email)
WHERE balance > 100 AND name LIKE 'A%';

WITH recent_orders AS (
    SELECT customer_id, COUNT(*) AS order_count, SUM(total) AS spent
    FROM orders
    WHERE order_date >= DATE '2024-01-01'
    GROUP BY customer_id
    HAVING COUNT(*) >= 3
)
SELECT c.id,
       c.name,
       r.order_count,
       r.spent,
       RANK() OVER (PARTITION BY c.id ORDER BY r.spent DESC) AS spend_rank,
       CASE WHEN r.spent > 1000 THEN 'gold' ELSE 'standard' END AS tier
FROM customers AS c
INNER JOIN recent_orders r ON r.customer_id = c.id
LEFT JOIN addresses a ON a.customer_id = c.id
WHERE c.email IS NOT NULL
  AND c.id IN (SELECT customer_id FROM vip_list)
  AND NOT c.balance <> 0
ORDER BY spent DESC, c.name ASC
LIMIT 10 OFFSET 20;

DELETE FROM customers WHERE id = 2;

DROP TABLE IF EXISTS temp_customers;
