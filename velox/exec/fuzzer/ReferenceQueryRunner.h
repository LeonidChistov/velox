/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <optional>
#include <set>
#include <unordered_map>

#include "velox/core/PlanNode.h"
#include "velox/expression/FunctionSignature.h"
#include "velox/vector/fuzzer/VectorFuzzer.h"

namespace facebook::velox::exec::test {

enum ReferenceQueryErrorCode {
  kSuccess,
  kReferenceQueryFail,
  kReferenceQueryUnsupported
};

/// Query runner that uses reference database, i.e. DuckDB, Presto, Spark.
class ReferenceQueryRunner {
 public:
  enum class RunnerType {
    kPrestoQueryRunner,
    kDuckQueryRunner,
    kSparkQueryRunner
  };

  // @param aggregatePool Used to allocate memory needed for vectors produced by
  // 'execute' methods.
  explicit ReferenceQueryRunner(memory::MemoryPool* aggregatePool)
      : aggregatePool_(aggregatePool) {}

  virtual ~ReferenceQueryRunner() = default;

  virtual RunnerType runnerType() const = 0;

  // Scalar types supported by the reference database, to be used to restrict
  // candidates when generating random types for fuzzers.
  virtual const std::vector<TypePtr>& supportedScalarTypes() const {
    return defaultScalarTypes();
  }

  virtual const std::unordered_map<std::string, DataSpec>&
  aggregationFunctionDataSpecs() const = 0;

  /// Converts Velox plan into SQL accepted by the reference database.
  /// @return std::nullopt if the plan uses features not supported by the
  /// reference database.
  virtual std::optional<std::string> toSql(const core::PlanNodePtr& plan) = 0;

  /// Same as the above toSql but for values nodes.
  virtual std::optional<std::string> toSql(
      const core::ValuesNodePtr& valuesNode);

  /// Same as the above toSql but for hash join nodes.
  virtual std::optional<std::string> toSql(
      const std::shared_ptr<const core::HashJoinNode>& joinNode);

  /// Same as the above toSql but for nested loop join nodes.
  virtual std::optional<std::string> toSql(
      const std::shared_ptr<const core::NestedLoopJoinNode>& joinNode);

  /// Returns whether a constant expression is supported by the reference
  /// database.
  virtual bool isConstantExprSupported(const core::TypedExprPtr& /*expr*/) {
    return true;
  }

  /// Returns whether types contained in a function signature are all supported
  /// by the reference database.
  virtual bool isSupported(const exec::FunctionSignature& /*signature*/) {
    return true;
  }

  /// Executes the plan and returns the result along with success or fail error
  /// code.
  virtual std::pair<
      std::optional<std::multiset<std::vector<velox::variant>>>,
      ReferenceQueryErrorCode>
  execute(const core::PlanNodePtr& /*plan*/) {
    VELOX_UNSUPPORTED();
  }

  /// Executes SQL query returned by the 'toSql' method using 'input' data.
  /// Converts results using 'resultType' schema.
  virtual std::multiset<std::vector<velox::variant>> execute(
      const std::string& sql,
      const std::vector<RowVectorPtr>& input,
      const RowTypePtr& resultType) = 0;

  /// Executes SQL query returned by the 'toSql' method using 'probeInput' and
  /// 'buildInput' data for join node.
  /// Converts results using 'resultType' schema.
  virtual std::multiset<std::vector<velox::variant>> execute(
      const std::string& /*sql*/,
      const std::vector<RowVectorPtr>& /*probeInput*/,
      const std::vector<RowVectorPtr>& /*buildInput*/,
      const RowTypePtr& /*resultType*/) {
    VELOX_UNSUPPORTED();
  }

  /// Returns true if 'executeVector' can be called to get results as Velox
  /// Vector.
  virtual bool supportsVeloxVectorResults() const {
    return false;
  }

  /// Similar to 'execute' but returns results in RowVector format.
  /// Caller should ensure 'supportsVeloxVectorResults' returns true.
  virtual std::vector<RowVectorPtr> executeVector(
      const std::string& /*sql*/,
      const std::vector<RowVectorPtr>& /*input*/,
      const RowTypePtr& /*resultType*/) {
    VELOX_UNSUPPORTED();
  }

  virtual std::vector<velox::RowVectorPtr> execute(const std::string& /*sql*/) {
    VELOX_UNSUPPORTED();
  }

  virtual std::vector<velox::RowVectorPtr> execute(
      const std::string& /*sql*/,
      const std::string& /*sessionProperty*/) {
    VELOX_UNSUPPORTED();
  }

 protected:
  memory::MemoryPool* aggregatePool() {
    return aggregatePool_;
  }

  bool isSupportedDwrfType(const TypePtr& type);

  /// Returns the name of the values node table in the form t_<id>.
  std::string getTableName(const core::ValuesNodePtr& valuesNode) {
    return fmt::format("t_{}", valuesNode->id());
  }

  // Traverses all nodes in the plan and returns all tables and their names.
  std::unordered_map<std::string, std::vector<velox::RowVectorPtr>>
  getAllTables(const core::PlanNodePtr& plan);

 private:
  memory::MemoryPool* aggregatePool_;

  std::optional<std::string> joinSourceToSql(const core::PlanNodePtr& planNode);
};
} // namespace facebook::velox::exec::test
